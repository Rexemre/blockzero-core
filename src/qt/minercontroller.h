// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MINERCONTROLLER_H
#define BITCOIN_QT_MINERCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QString>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
QT_END_NAMESPACE

/** Run-time configuration for a single mining session. */
struct MinerConfig {
    //! Absolute path to the (verified) xmrig binary.
    QString minerPath;
    //! Pool stratum endpoint, host:port (e.g. "pool.bloz.org:3334").
    QString poolUrl;
    //! Block Zero payout address (bz1q...). Rewards are paid here by the pool.
    QString address;
    //! Per-machine worker label shown on the pool dashboard.
    QString worker;
    //! Mining threads. 0 lets xmrig auto-detect the best count.
    int threads{0};
};

/** Live statistics scraped from the local xmrig HTTP API. */
struct MinerStats {
    double hashrate_10s{0.0};
    double hashrate_60s{0.0};
    double hashrate_15m{0.0};
    int shares_good{0};
    int shares_total{0};
    bool connected{false};
};

/**
 * MinerController manages an *external* miner process (patched XMRig,
 * algorithm rx/blockzero) on behalf of the GUI wallet.
 *
 * Design notes (security):
 *  - The miner runs as a separate child process and never has access to the
 *    wallet, its keys or its passphrase. It only needs a payout address.
 *  - The process is always terminated when the controller is destroyed, so
 *    closing the wallet never leaves a "zombie" miner running.
 *  - The xmrig HTTP API is bound to 127.0.0.1 on a random port and guarded by
 *    a random per-session bearer token, so it is never reachable externally.
 *
 * This class is intentionally wallet-agnostic: callers pass a fully resolved
 * MinerConfig (including the payout address), which keeps it easy to test.
 */
class MinerController : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error,
    };

    explicit MinerController(QObject* parent = nullptr);
    ~MinerController();

    //! Replace the configuration used for the next start(). Ignored while running.
    void setConfig(const MinerConfig& config);
    const MinerConfig& config() const { return m_config; }

    State state() const { return m_state; }
    bool isActive() const { return m_state == State::Starting || m_state == State::Running; }

    const MinerStats& lastStats() const { return m_stats; }

public Q_SLOTS:
    //! Launch the miner with the current configuration.
    void start();
    //! Gracefully stop the miner (terminate, then kill after a grace period).
    void stop();

Q_SIGNALS:
    void stateChanged(MinerController::State state);
    void statsUpdated(const MinerStats& stats);
    void logLine(const QString& line);
    void errorOccurred(const QString& message);

private Q_SLOTS:
    void handleProcessStarted();
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exit_code, QProcess::ExitStatus status);
    void handleReadyRead();
    void pollStats();
    void handleStatsReply(QNetworkReply* reply);

private:
    void setState(State state);
    QStringList buildArguments() const;

    MinerConfig m_config;
    State m_state{State::Stopped};
    MinerStats m_stats;

    QProcess* m_process{nullptr};
    QNetworkAccessManager* m_net{nullptr};
    QTimer* m_pollTimer{nullptr};

    //! Local xmrig HTTP API coordinates (randomized per session).
    quint16 m_httpPort{0};
    QString m_httpToken;

    //! Set while a user-requested stop is in progress so the finished handler
    //! does not report it as a crash / error.
    bool m_stopRequested{false};
};

#endif // BITCOIN_QT_MINERCONTROLLER_H
