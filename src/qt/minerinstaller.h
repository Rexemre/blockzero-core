// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MINERINSTALLER_H
#define BITCOIN_QT_MINERINSTALLER_H

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
QT_END_NAMESPACE

/**
 * MinerInstaller fetches the patched XMRig build on demand and verifies it
 * before it is ever executed.
 *
 * Security model:
 *  - The wallet installer itself ships WITHOUT a miner, so the wallet binary
 *    keeps a clean antivirus / code-signing reputation. The miner is only
 *    downloaded when the user explicitly opts in to mining.
 *  - The download is pinned to a specific release tag and its SHA-256 is
 *    verified against a hash baked into this source file. A tampered or
 *    man-in-the-middled archive is rejected before extraction.
 *  - The verified binary is extracted into a per-version directory inside the
 *    application data location, so an already-installed version is reused
 *    without re-downloading.
 *
 * Note: bumping the bundled miner version requires updating both the tag and
 * the per-platform SHA-256 constants in minerinstaller.cpp.
 */
class MinerInstaller : public QObject
{
    Q_OBJECT

public:
    explicit MinerInstaller(QObject* parent = nullptr);
    ~MinerInstaller();

    //! True if a miner build exists for the current OS/architecture.
    static bool isPlatformSupported();

    //! Path to the already-verified miner binary, or empty if not installed yet.
    QString installedMinerPath() const;

public Q_SLOTS:
    //! Ensure a verified miner is present, downloading & extracting if needed.
    //! Emits finished() with the binary path on success, or failed() on error.
    void ensureInstalled();

Q_SIGNALS:
    void statusMessage(const QString& message);
    void downloadProgress(qint64 received, qint64 total);
    void finished(const QString& minerPath);
    void failed(const QString& error);

private Q_SLOTS:
    void handleDownloadReadyRead();
    void handleDownloadFinished();

private:
    QString versionDir() const;
    QString binaryPath() const;
    void startDownload();
    void cleanupReply();
    bool verifyChecksum(const QString& filePath) const;
    bool extractArchive(const QString& archivePath, const QString& destDir, QString& error) const;
    static QString locateBinary(const QString& dir);

    QNetworkAccessManager* m_net{nullptr};
    QNetworkReply* m_reply{nullptr};
    QFile* m_outFile{nullptr};
    QString m_archivePath;
};

#endif // BITCOIN_QT_MINERINSTALLER_H
