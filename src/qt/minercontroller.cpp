// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/minercontroller.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>

namespace {
//! How often to scrape the xmrig HTTP API for live stats.
constexpr int STATS_POLL_INTERVAL_MS = 2000;
//! Grace period between terminate() and a forceful kill() on stop().
constexpr int STOP_GRACE_MS = 5000;
//! Mining algorithm implemented by the patched (bz) xmrig build.
constexpr char MINING_ALGO[] = "rx/blockzero";
} // namespace

MinerController::MinerController(QObject* parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::started, this, &MinerController::handleProcessStarted);
    connect(m_process, &QProcess::errorOccurred, this, &MinerController::handleProcessError);
    connect(m_process, &QProcess::finished, this, &MinerController::handleProcessFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MinerController::handleReadyRead);

    m_net = new QNetworkAccessManager(this);
    connect(m_net, &QNetworkAccessManager::finished, this, &MinerController::handleStatsReply);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(STATS_POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &MinerController::pollStats);
}

MinerController::~MinerController()
{
    // Never leave a mining process behind when the wallet shuts down.
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void MinerController::setConfig(const MinerConfig& config)
{
    if (isActive()) return; // configuration is locked while mining
    m_config = config;
}

void MinerController::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

QStringList MinerController::buildArguments() const
{
    const QString user = m_config.worker.isEmpty()
                             ? m_config.address
                             : m_config.address + "." + m_config.worker;

    QStringList args{
        "-a", MINING_ALGO,
        "-o", m_config.poolUrl,
        "-u", user,
        "-p", "x",
        // No donation to xmrig's developers; the on-chain dev fund is separate.
        "--donate-level", "0",
        "--no-color",
        // Local-only HTTP API for live stats, guarded by a random bearer token.
        "--http-host", "127.0.0.1",
        "--http-port", QString::number(m_httpPort),
        "--http-access-token", m_httpToken,
    };
    if (m_config.threads > 0) {
        args << "-t" << QString::number(m_config.threads);
    }
    return args;
}

void MinerController::start()
{
    if (isActive()) return;

    if (m_config.minerPath.isEmpty() || m_config.address.isEmpty() || m_config.poolUrl.isEmpty()) {
        Q_EMIT errorOccurred(tr("Mining is not configured yet."));
        setState(State::Error);
        return;
    }

    // Randomize the local HTTP API endpoint per session (defence in depth).
    m_httpPort = static_cast<quint16>(28000 + QRandomGenerator::system()->bounded(20000));
    m_httpToken = QString::number(QRandomGenerator::system()->generate64(), 16);

    m_stopRequested = false;
    m_stats = MinerStats{};
    setState(State::Starting);

    m_process->start(m_config.minerPath, buildArguments());
}

void MinerController::stop()
{
    if (!isActive()) return;

    m_stopRequested = true;
    setState(State::Stopping);
    m_pollTimer->stop();

    m_process->terminate();
    // Console miners may ignore terminate() on Windows; force-kill after a grace period.
    QTimer::singleShot(STOP_GRACE_MS, this, [this]() {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    });
}

void MinerController::handleProcessStarted()
{
    setState(State::Running);
    m_pollTimer->start();
}

void MinerController::handleProcessError(QProcess::ProcessError error)
{
    if (m_stopRequested) return; // expected during a user-requested stop

    QString message;
    switch (error) {
    case QProcess::FailedToStart:
        message = tr("The miner failed to start. The file may be missing or blocked by antivirus.");
        break;
    case QProcess::Crashed:
        message = tr("The miner crashed unexpectedly.");
        break;
    default:
        message = tr("A miner process error occurred.");
        break;
    }
    Q_EMIT errorOccurred(message);
    setState(State::Error);
}

void MinerController::handleProcessFinished(int exit_code, QProcess::ExitStatus status)
{
    m_pollTimer->stop();

    const bool clean_stop = m_stopRequested;
    m_stopRequested = false;

    // Reset live stats now that nothing is mining.
    m_stats = MinerStats{};
    Q_EMIT statsUpdated(m_stats);

    if (clean_stop) {
        setState(State::Stopped);
        return;
    }

    if (status == QProcess::CrashExit || exit_code != 0) {
        Q_EMIT errorOccurred(tr("The miner stopped unexpectedly (exit code %1).").arg(exit_code));
        setState(State::Error);
    } else {
        setState(State::Stopped);
    }
}

void MinerController::handleReadyRead()
{
    const QByteArray chunk = m_process->readAllStandardOutput();
    const QList<QByteArray> lines = chunk.split('\n');
    for (const QByteArray& raw : lines) {
        const QString line = QString::fromLocal8Bit(raw).trimmed();
        if (!line.isEmpty()) {
            Q_EMIT logLine(line);
        }
    }
}

void MinerController::pollStats()
{
    if (m_state != State::Running || m_httpPort == 0) return;

    QUrl url(QStringLiteral("http://127.0.0.1:%1/2/summary").arg(m_httpPort));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_httpToken.toUtf8());
    m_net->get(request);
}

void MinerController::handleStatsReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return; // API may not be ready yet during startup; ignore transient errors
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) return;
    const QJsonObject root = doc.object();

    MinerStats stats;

    const QJsonArray total = root.value("hashrate").toObject().value("total").toArray();
    if (total.size() >= 1) stats.hashrate_10s = total.at(0).toDouble();
    if (total.size() >= 2) stats.hashrate_60s = total.at(1).toDouble();
    if (total.size() >= 3) stats.hashrate_15m = total.at(2).toDouble();

    const QJsonObject results = root.value("results").toObject();
    stats.shares_good = results.value("shares_good").toInt();
    stats.shares_total = results.value("shares_total").toInt();

    const QJsonObject connection = root.value("connection").toObject();
    stats.connected = connection.value("uptime").toInt() > 0;

    m_stats = stats;
    Q_EMIT statsUpdated(m_stats);
}
