// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpage.h>

#include <qt/addresstablemodel.h>
#include <qt/minerinstaller.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <interfaces/wallet.h>
#include <outputtype.h>

#include <algorithm>

#include <QCheckBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

//! How often to re-check AC/battery power state.
constexpr int POWER_POLL_INTERVAL_MS = 15000;

//! True if the device is currently running on battery (Windows only; other
//! platforms report false so the pause-on-battery option is simply inert).
bool OnBatteryPower()
{
#ifdef Q_OS_WIN
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        // ACLineStatus: 0 = offline (battery), 1 = online (AC), 255 = unknown.
        return status.ACLineStatus == 0;
    }
#endif
    return false;
}

//! Official pool stratum endpoint. Hardcoded so mining can never be silently
//! pointed at a third-party / malicious pool via configuration.
constexpr char POOL_URL[] = "pool.bloz.org:3334";

//! Pool REST endpoint used to display accumulated earnings for an address.
constexpr char POOL_WORKER_API[] = "https://pool.bloz.org/api/worker";

//! How often to refresh earnings from the pool API.
constexpr int EARNINGS_POLL_INTERVAL_MS = 30000;

//! Satoshis per BLOZ (same 8-decimal convention as the base chain).
constexpr double COIN_SATS = 100000000.0;

QString FormatBloz(qint64 sats)
{
    return QString::number(sats / COIN_SATS, 'f', 8) + " BLOZ";
}

QString FormatHashrate(double hps)
{
    if (hps <= 0.0) return QObject::tr("0 H/s");
    const char* units[] = {"H/s", "kH/s", "MH/s", "GH/s"};
    int u = 0;
    while (hps >= 1000.0 && u < 3) {
        hps /= 1000.0;
        ++u;
    }
    return QString::number(hps, 'f', u == 0 ? 0 : 2) + " " + units[u];
}
} // namespace

MiningPage::MiningPage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent)
{
    m_controller = new MinerController(this);
    m_installer = new MinerInstaller(this);
    m_earningsNet = new QNetworkAccessManager(this);

    setupUi(platformStyle);

    connect(m_controller, &MinerController::stateChanged, this, &MiningPage::onControllerState);
    connect(m_controller, &MinerController::statsUpdated, this, &MiningPage::onStats);
    connect(m_controller, &MinerController::logLine, this, &MiningPage::onLogLine);
    connect(m_controller, &MinerController::errorOccurred, this, &MiningPage::onControllerError);

    connect(m_installer, &MinerInstaller::statusMessage, this, &MiningPage::onInstallerStatus);
    connect(m_installer, &MinerInstaller::downloadProgress, this, &MiningPage::onInstallerProgress);
    connect(m_installer, &MinerInstaller::finished, this, &MiningPage::onInstallerFinished);
    connect(m_installer, &MinerInstaller::failed, this, &MiningPage::onInstallerFailed);

    connect(m_earningsNet, &QNetworkAccessManager::finished, this, &MiningPage::handleEarningsReply);
    m_earningsTimer = new QTimer(this);
    m_earningsTimer->setInterval(EARNINGS_POLL_INTERVAL_MS);
    connect(m_earningsTimer, &QTimer::timeout, this, &MiningPage::pollEarnings);
    m_earningsTimer->start();

    m_powerTimer = new QTimer(this);
    m_powerTimer->setInterval(POWER_POLL_INTERVAL_MS);
    connect(m_powerTimer, &QTimer::timeout, this, &MiningPage::checkPowerState);
    m_powerTimer->start();

    updateControls();
}

MiningPage::~MiningPage() = default;

void MiningPage::setupUi(const PlatformStyle* platformStyle)
{
    Q_UNUSED(platformStyle);
    auto* outer = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Mine BLOZ with your CPU. Rewards are paid directly to this wallet. "
                                "Mining uses your processor and electricity; you can stop at any time."));
    intro->setWordWrap(true);
    outer->addWidget(intro);

    // --- Status group -------------------------------------------------------
    auto* statusGroup = new QGroupBox(tr("Status"));
    auto* statusForm = new QVBoxLayout(statusGroup);

    auto addRow = [](QVBoxLayout* parent, const QString& caption, QLabel** valueOut) {
        auto* row = new QHBoxLayout();
        auto* cap = new QLabel(caption);
        cap->setMinimumWidth(140);
        auto* val = new QLabel("—");
        row->addWidget(cap);
        row->addWidget(val, 1);
        parent->addLayout(row);
        *valueOut = val;
    };

    addRow(statusForm, tr("State:"), &m_statusValue);
    addRow(statusForm, tr("Hashrate:"), &m_hashrateValue);
    addRow(statusForm, tr("Accepted shares:"), &m_sharesValue);
    addRow(statusForm, tr("Pending balance:"), &m_balanceValue);
    addRow(statusForm, tr("Paid out:"), &m_paidValue);

    QFont hashFont = m_hashrateValue->font();
    hashFont.setPointSizeF(hashFont.pointSizeF() * 1.6);
    hashFont.setBold(true);
    m_hashrateValue->setFont(hashFont);

    outer->addWidget(statusGroup);

    // --- Payout address -----------------------------------------------------
    auto* addrGroup = new QGroupBox(tr("Payout address"));
    auto* addrLayout = new QHBoxLayout(addrGroup);
    m_addressValue = new QLabel(tr("(created automatically when you start)"));
    m_addressValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont mono("monospace");
    mono.setStyleHint(QFont::TypeWriter);
    m_addressValue->setFont(mono);
    m_newAddressButton = new QPushButton(tr("New address"));
    addrLayout->addWidget(m_addressValue, 1);
    addrLayout->addWidget(m_newAddressButton);
    outer->addWidget(addrGroup);

    // --- Intensity ----------------------------------------------------------
    auto* intensityGroup = new QGroupBox(tr("CPU usage"));
    auto* intensityLayout = new QHBoxLayout(intensityGroup);
    const int maxThreads = std::max(1, QThread::idealThreadCount());
    m_intensitySlider = new QSlider(Qt::Horizontal);
    m_intensitySlider->setMinimum(1);
    m_intensitySlider->setMaximum(maxThreads);
    m_intensitySlider->setValue(std::max(1, maxThreads / 2));
    m_intensityValue = new QLabel();
    intensityLayout->addWidget(new QLabel(tr("Threads:")));
    intensityLayout->addWidget(m_intensitySlider, 1);
    intensityLayout->addWidget(m_intensityValue);
    outer->addWidget(intensityGroup);

    auto updateIntensityLabel = [this, maxThreads]() {
        m_intensityValue->setText(tr("%1 of %2").arg(m_intensitySlider->value()).arg(maxThreads));
    };
    connect(m_intensitySlider, &QSlider::valueChanged, this, updateIntensityLabel);
    updateIntensityLabel();

    // --- Controls -----------------------------------------------------------
    m_toggleButton = new QPushButton(tr("Start mining"));
    m_toggleButton->setMinimumHeight(36);
    outer->addWidget(m_toggleButton);

    m_autoStartCheck = new QCheckBox(tr("Start mining automatically when this wallet opens"));
    outer->addWidget(m_autoStartCheck);

    m_batteryPauseCheck = new QCheckBox(tr("Pause mining while running on battery power"));
    outer->addWidget(m_batteryPauseCheck);

    m_showLogCheck = new QCheckBox(tr("Show miner log"));
    outer->addWidget(m_showLogCheck);

    m_log = new QPlainTextEdit();
    m_log->setReadOnly(true);
    m_log->setVisible(false);
    m_log->setMaximumBlockCount(500);
    outer->addWidget(m_log, 1);

    connect(m_showLogCheck, &QCheckBox::toggled, m_log, &QPlainTextEdit::setVisible);
    connect(m_toggleButton, &QPushButton::clicked, this, &MiningPage::toggleMining);
    connect(m_newAddressButton, &QPushButton::clicked, this, &MiningPage::generateNewAddress);
    connect(m_intensitySlider, &QSlider::valueChanged, this, &MiningPage::saveSettings);
    connect(m_autoStartCheck, &QCheckBox::toggled, this, &MiningPage::saveSettings);
    connect(m_batteryPauseCheck, &QCheckBox::toggled, this, &MiningPage::saveSettings);
    connect(m_batteryPauseCheck, &QCheckBox::toggled, this, &MiningPage::checkPowerState);

    outer->addStretch();
}

void MiningPage::setWalletModel(WalletModel* walletModel)
{
    m_walletModel = walletModel;
    loadSettings();
    if (!m_payoutAddress.isEmpty()) {
        m_addressValue->setText(m_payoutAddress);
        pollEarnings();
    }
    maybeAutoStart();
}

QString MiningPage::payoutSettingsKey() const
{
    // Address is keyed per wallet so opening a different wallet never reuses
    // another wallet's mining address.
    const QString wallet = m_walletModel ? m_walletModel->getWalletName() : QString();
    return QString("mining/payoutAddress/%1").arg(wallet);
}

void MiningPage::loadSettings()
{
    QSettings settings;
    const int threads = settings.value("mining/threads", m_intensitySlider->value()).toInt();
    m_intensitySlider->setValue(std::clamp(threads, m_intensitySlider->minimum(), m_intensitySlider->maximum()));
    m_autoStartCheck->setChecked(settings.value("mining/autoStart", false).toBool());
    m_batteryPauseCheck->setChecked(settings.value("mining/pauseOnBattery", true).toBool());
    m_payoutAddress = settings.value(payoutSettingsKey()).toString();
}

void MiningPage::saveSettings()
{
    QSettings settings;
    settings.setValue("mining/threads", m_intensitySlider->value());
    settings.setValue("mining/autoStart", m_autoStartCheck->isChecked());
    settings.setValue("mining/pauseOnBattery", m_batteryPauseCheck->isChecked());
    if (!m_payoutAddress.isEmpty()) {
        settings.setValue(payoutSettingsKey(), m_payoutAddress);
    }
}

void MiningPage::maybeAutoStart()
{
    // Only auto-start when the user opted in, a payout address already exists,
    // and a verified miner is already installed (never silently download here).
    if (!m_autoStartCheck->isChecked()) return;
    if (!MinerInstaller::isPlatformSupported()) return;
    if (m_payoutAddress.isEmpty()) return;
    if (m_installer->installedMinerPath().isEmpty()) return;
    beginStart();
}

bool MiningPage::ensurePayoutAddress()
{
    if (!m_payoutAddress.isEmpty()) return true;
    if (!m_walletModel || !m_walletModel->getAddressTableModel()) return false;

    const OutputType type = m_walletModel->wallet().getDefaultAddressType();
    const QString addr = m_walletModel->getAddressTableModel()->addRow(
        AddressTableModel::Receive, tr("Mining rewards"), "", type);

    if (addr.isEmpty()) {
        QMessageBox::warning(this, tr("Mining"),
            tr("Could not create a payout address. If your wallet is encrypted, please unlock it first."));
        return false;
    }
    m_payoutAddress = addr;
    m_addressValue->setText(addr);
    saveSettings();
    pollEarnings();
    return true;
}

void MiningPage::generateNewAddress()
{
    m_payoutAddress.clear();
    if (ensurePayoutAddress()) {
        onLogLine(tr("New payout address: %1").arg(m_payoutAddress));
    }
}

bool MiningPage::confirmMiningOptIn()
{
    const auto answer = QMessageBox::question(this, tr("Start mining?"),
        tr("Mining will use your CPU and electricity, and may make your computer warmer and slower.\n\n"
           "Antivirus software commonly flags CPU miners as \"riskware\" — this is expected for every miner. "
           "The miner is downloaded from the official Block Zero release and verified before it runs.\n\n"
           "Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void MiningPage::toggleMining()
{
    if (m_controller->isActive()) {
        // Explicit user stop overrides any automatic battery pause.
        m_batteryPaused = false;
        m_controller->stop();
        return;
    }

    if (!MinerInstaller::isPlatformSupported()) {
        QMessageBox::warning(this, tr("Mining"),
            tr("Mining is not available for this operating system or architecture."));
        return;
    }
    if (!ensurePayoutAddress()) return;
    if (!confirmMiningOptIn()) return;

    beginStart();
}

void MiningPage::beginStart()
{
    m_pendingStart = true;
    m_toggleButton->setEnabled(false);
    m_toggleButton->setText(tr("Preparing…"));
    // Downloads + verifies on first use; returns immediately if already installed.
    m_installer->ensureInstalled();
}

void MiningPage::onInstallerStatus(const QString& message)
{
    m_statusValue->setText(message);
    onLogLine(message);
}

void MiningPage::onInstallerProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_statusValue->setText(tr("Downloading miner… %1%").arg(received * 100 / total));
    }
}

void MiningPage::onInstallerFinished(const QString& minerPath)
{
    MinerConfig cfg;
    cfg.minerPath = minerPath;
    cfg.poolUrl = POOL_URL;
    cfg.address = m_payoutAddress;
    cfg.worker = QSysInfo::machineHostName();
    cfg.threads = m_intensitySlider->value();
    m_controller->setConfig(cfg);

    if (m_pendingStart) {
        m_pendingStart = false;
        m_controller->start();
    }
}

void MiningPage::onInstallerFailed(const QString& error)
{
    m_pendingStart = false;
    QMessageBox::warning(this, tr("Mining"), error);
    onLogLine(error);
    updateControls();
}

void MiningPage::onControllerState(MinerController::State /*state*/)
{
    updateControls();
}

void MiningPage::onStats(const MinerStats& stats)
{
    m_hashrateValue->setText(FormatHashrate(stats.hashrate_10s));
    m_sharesValue->setText(QString("%1 / %2").arg(stats.shares_good).arg(stats.shares_total));
}

void MiningPage::onLogLine(const QString& line)
{
    if (m_log) m_log->appendPlainText(line);
}

void MiningPage::onControllerError(const QString& message)
{
    onLogLine(message);
}

void MiningPage::pollEarnings()
{
    if (m_payoutAddress.isEmpty()) return;

    QUrl url(POOL_WORKER_API);
    QUrlQuery query;
    query.addQueryItem("address", m_payoutAddress);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_earningsNet->get(request);
}

void MiningPage::handleEarningsReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) return;
    const QJsonObject miner = doc.object().value("miner").toObject();
    if (miner.isEmpty()) return;

    const QJsonObject balance = miner.value("balance").toObject();
    const qint64 immature = balance.value("immature_sats").toVariant().toLongLong();
    const qint64 pending = balance.value("pending_sats").toVariant().toLongLong();
    const qint64 paid = balance.value("paid_sats").toVariant().toLongLong();

    m_balanceValue->setText(FormatBloz(immature + pending));
    m_paidValue->setText(FormatBloz(paid));
}

void MiningPage::checkPowerState()
{
    if (!m_batteryPauseCheck->isChecked()) return;

    const bool on_battery = OnBatteryPower();

    if (on_battery && m_controller->isActive() && !m_batteryPaused) {
        // Auto-pause: remember that we stopped for power so we can resume.
        m_batteryPaused = true;
        onLogLine(tr("On battery power — mining paused."));
        m_statusValue->setText(tr("Paused (on battery)"));
        m_controller->stop();
    } else if (!on_battery && m_batteryPaused) {
        m_batteryPaused = false;
        if (!m_payoutAddress.isEmpty() && !m_installer->installedMinerPath().isEmpty()) {
            onLogLine(tr("On AC power — resuming mining."));
            beginStart();
        }
    }
}

void MiningPage::updateControls()
{
    const MinerController::State state = m_controller->state();
    const bool active = m_controller->isActive();

    m_intensitySlider->setEnabled(!active && !m_pendingStart);
    m_newAddressButton->setEnabled(!active && !m_pendingStart);
    m_toggleButton->setEnabled(!m_pendingStart);

    switch (state) {
    case MinerController::State::Stopped:
        m_statusValue->setText(tr("Stopped"));
        m_hashrateValue->setText(FormatHashrate(0));
        m_toggleButton->setText(tr("Start mining"));
        break;
    case MinerController::State::Starting:
        m_statusValue->setText(tr("Starting…"));
        m_toggleButton->setText(tr("Stop mining"));
        m_toggleButton->setEnabled(true);
        break;
    case MinerController::State::Running:
        m_statusValue->setText(tr("Mining"));
        m_toggleButton->setText(tr("Stop mining"));
        m_toggleButton->setEnabled(true);
        break;
    case MinerController::State::Stopping:
        m_statusValue->setText(tr("Stopping…"));
        m_toggleButton->setText(tr("Stopping…"));
        m_toggleButton->setEnabled(false);
        break;
    case MinerController::State::Error:
        m_statusValue->setText(tr("Error — see log"));
        m_toggleButton->setText(tr("Start mining"));
        break;
    }
}
