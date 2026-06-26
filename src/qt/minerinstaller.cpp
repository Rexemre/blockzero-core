// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/minerinstaller.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace {
//! GitHub repository and pinned release that publishes the patched xmrig.
constexpr char MINER_REPO[] = "Rexemre/blockzero-ops";
constexpr char MINER_TAG[] = "xmrig-v6.26.0-bz4";

struct PlatformAsset {
    const char* asset;   //!< release asset file name
    const char* sha256;  //!< expected lowercase hex SHA-256 of that asset
    const char* binary;  //!< miner executable name inside the archive
};

//! Per-platform asset + pinned SHA-256. Update together when bumping MINER_TAG.
#if defined(Q_OS_WIN)
constexpr PlatformAsset PLATFORM{
    "bz-xmrig-windows-x64.zip",
    "a40f2d2052a21ebe2d6c8418e67e62d52ce986c9ba3de9ff2ce8ead6254eb9dc",
    "xmrig.exe",
};
#elif defined(Q_OS_MACOS)
constexpr PlatformAsset PLATFORM{
    "bz-xmrig-macos-arm64.tar.gz",
    "9554536452bbe3dad07949a97f94ec1275b292f43c1adba06b9da8788fadff80",
    "xmrig",
};
#elif defined(Q_OS_LINUX)
constexpr PlatformAsset PLATFORM{
    "bz-xmrig-linux-x64.tar.gz",
    "f081e63dcb9da7288216834815ec746084263fd60e6826929fc8f8509bf98d8e",
    "xmrig",
};
#else
constexpr PlatformAsset PLATFORM{nullptr, nullptr, nullptr};
#endif
} // namespace

MinerInstaller::MinerInstaller(QObject* parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

MinerInstaller::~MinerInstaller()
{
    cleanupReply();
}

bool MinerInstaller::isPlatformSupported()
{
    return PLATFORM.asset != nullptr;
}

QString MinerInstaller::versionDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QString("miner/") + MINER_TAG);
}

QString MinerInstaller::binaryPath() const
{
    if (!isPlatformSupported()) return QString();
    return locateBinary(versionDir());
}

QString MinerInstaller::installedMinerPath() const
{
    const QString path = binaryPath();
    if (!path.isEmpty() && QFileInfo::exists(path)) return path;
    return QString();
}

QString MinerInstaller::locateBinary(const QString& dir)
{
    if (!isPlatformSupported()) return QString();
    // The binary may live at the archive root or in a subdirectory.
    QDirIterator it(dir, QStringList{QString::fromLatin1(PLATFORM.binary)},
                    QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) return it.next();
    return QString();
}

void MinerInstaller::ensureInstalled()
{
    if (!isPlatformSupported()) {
        Q_EMIT failed(tr("Mining is not available for this operating system or architecture."));
        return;
    }

    const QString existing = installedMinerPath();
    if (!existing.isEmpty()) {
        Q_EMIT finished(existing);
        return;
    }

    QDir().mkpath(versionDir());
    startDownload();
}

void MinerInstaller::startDownload()
{
    const QString url = QString("https://github.com/%1/releases/download/%2/%3")
                            .arg(MINER_REPO, MINER_TAG, PLATFORM.asset);

    m_archivePath = QDir(versionDir()).filePath(PLATFORM.asset);
    m_outFile = new QFile(m_archivePath, this);
    if (!m_outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT failed(tr("Could not write to the miner download location."));
        m_outFile->deleteLater();
        m_outFile = nullptr;
        return;
    }

    Q_EMIT statusMessage(tr("Downloading miner (%1)…").arg(MINER_TAG));

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &MinerInstaller::handleDownloadReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &MinerInstaller::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &MinerInstaller::handleDownloadFinished);
}

void MinerInstaller::handleDownloadReadyRead()
{
    if (m_reply && m_outFile) {
        m_outFile->write(m_reply->readAll());
    }
}

void MinerInstaller::cleanupReply()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_outFile) {
        m_outFile->close();
        m_outFile->deleteLater();
        m_outFile = nullptr;
    }
}

void MinerInstaller::handleDownloadFinished()
{
    if (!m_reply || !m_outFile) return;

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = m_reply->errorString();
        cleanupReply();
        QFile::remove(m_archivePath);
        Q_EMIT failed(tr("Download failed: %1").arg(err));
        return;
    }

    // Flush any remaining buffered bytes before hashing.
    m_outFile->write(m_reply->readAll());
    m_outFile->close();
    cleanupReply();

    Q_EMIT statusMessage(tr("Verifying download…"));
    if (!verifyChecksum(m_archivePath)) {
        QFile::remove(m_archivePath);
        Q_EMIT failed(tr("Verification failed: the downloaded miner does not match the expected checksum. "
                         "It was deleted for your safety."));
        return;
    }

    Q_EMIT statusMessage(tr("Extracting miner…"));
    QString extractError;
    if (!extractArchive(m_archivePath, versionDir(), extractError)) {
        Q_EMIT failed(tr("Could not extract the miner: %1").arg(extractError));
        return;
    }
    QFile::remove(m_archivePath); // archive no longer needed

    const QString binary = installedMinerPath();
    if (binary.isEmpty()) {
        Q_EMIT failed(tr("The miner binary was not found after extraction."));
        return;
    }

#ifndef Q_OS_WIN
    // Ensure the extracted binary is executable.
    QFile bin(binary);
    bin.setPermissions(bin.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif

    Q_EMIT finished(binary);
}

bool MinerInstaller::verifyChecksum(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return false;

    const QString actual = QString::fromLatin1(hash.result().toHex());
    return actual.compare(QString::fromLatin1(PLATFORM.sha256), Qt::CaseInsensitive) == 0;
}

bool MinerInstaller::extractArchive(const QString& archivePath, const QString& destDir, QString& error) const
{
    // bsdtar/libarchive (Windows 10 1803+, macOS) and GNU tar (Linux) all
    // auto-detect and extract both .zip and .tar.gz with the same invocation.
    QProcess tar;
    tar.setWorkingDirectory(destDir);
    tar.start("tar", QStringList{"-xf", archivePath, "-C", destDir});
    if (!tar.waitForStarted(5000)) {
        error = tr("the 'tar' utility is not available");
        return false;
    }
    if (!tar.waitForFinished(120000)) {
        error = tr("extraction timed out");
        return false;
    }
    if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
        error = QString::fromLocal8Bit(tar.readAllStandardError()).trimmed();
        if (error.isEmpty()) error = tr("unknown extraction error");
        return false;
    }
    return true;
}
