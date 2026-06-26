// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MININGPAGE_H
#define BITCOIN_QT_MININGPAGE_H

#include <qt/minercontroller.h>

#include <QWidget>

class PlatformStyle;
class WalletModel;
class MinerInstaller;
struct MinerStats;

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QPushButton;
class QSlider;
class QPlainTextEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
QT_END_NAMESPACE

/**
 * MiningPage is the user-facing "Mining" tab. It owns a MinerController (the
 * external miner process) and a MinerInstaller (on-demand, verified download),
 * and mines to a freshly generated receiving address of the current wallet.
 *
 * The user only ever interacts with a single Start/Stop button plus an
 * intensity slider; address handling and the miner download happen
 * automatically.
 */
class MiningPage : public QWidget
{
    Q_OBJECT

public:
    explicit MiningPage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~MiningPage();

    void setWalletModel(WalletModel* walletModel);

private Q_SLOTS:
    void toggleMining();
    void generateNewAddress();
    void onControllerState(MinerController::State state);
    void onStats(const MinerStats& stats);
    void onLogLine(const QString& line);
    void onControllerError(const QString& message);
    void onInstallerStatus(const QString& message);
    void onInstallerProgress(qint64 received, qint64 total);
    void onInstallerFinished(const QString& minerPath);
    void onInstallerFailed(const QString& error);
    void pollEarnings();
    void handleEarningsReply(QNetworkReply* reply);
    void checkPowerState();

private:
    void setupUi(const PlatformStyle* platformStyle);
    bool ensurePayoutAddress();
    bool confirmMiningOptIn();
    void beginStart();
    void updateControls();
    void loadSettings();
    void saveSettings();
    void maybeAutoStart();
    QString payoutSettingsKey() const;

    WalletModel* m_walletModel{nullptr};
    MinerController* m_controller{nullptr};
    MinerInstaller* m_installer{nullptr};
    QNetworkAccessManager* m_earningsNet{nullptr};
    QTimer* m_earningsTimer{nullptr};
    QTimer* m_powerTimer{nullptr};

    QString m_payoutAddress;
    bool m_pendingStart{false};
    //! True while mining is auto-paused because the device is on battery.
    bool m_batteryPaused{false};

    QLabel* m_statusValue{nullptr};
    QLabel* m_hashrateValue{nullptr};
    QLabel* m_sharesValue{nullptr};
    QLabel* m_balanceValue{nullptr};
    QLabel* m_paidValue{nullptr};
    QLabel* m_addressValue{nullptr};
    QLabel* m_intensityValue{nullptr};
    QSlider* m_intensitySlider{nullptr};
    QPushButton* m_toggleButton{nullptr};
    QPushButton* m_newAddressButton{nullptr};
    QCheckBox* m_autoStartCheck{nullptr};
    QCheckBox* m_batteryPauseCheck{nullptr};
    QCheckBox* m_showLogCheck{nullptr};
    QPlainTextEdit* m_log{nullptr};
};

#endif // BITCOIN_QT_MININGPAGE_H
