// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/updatechecker.h>

#include <clientversion.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {
const char* const RELEASES_API = "https://api.github.com/repos/Rexemre/blockzero-core/releases/latest";
const char* const RELEASES_PAGE = "https://github.com/Rexemre/blockzero-core/releases/latest";
const char* const INSTALLER_NAME = "blockzero-windows-x64-setup.exe";
constexpr int NETWORK_TIMEOUT_MS = 20000;
constexpr qint64 CHECK_INTERVAL_SECS = 20 * 60 * 60; // "once a day" with some slack for daily users

struct Version {
    int major{0}, minor{0}, build{0}, rc{0};
    bool valid{false};
};

Version parseTag(const QString& tag)
{
    static const QRegularExpression re(QStringLiteral("^v?(\\d+)\\.(\\d+)\\.(\\d+)(?:-rc(\\d+))?$"));
    Version v;
    const auto m = re.match(tag.trimmed());
    if (!m.hasMatch()) return v;
    v.major = m.captured(1).toInt();
    v.minor = m.captured(2).toInt();
    v.build = m.captured(3).toInt();
    v.rc = m.captured(4).isEmpty() ? 0 : m.captured(4).toInt();
    v.valid = true;
    return v;
}

QString userAgent()
{
    const QString tag = UpdateChecker::currentTag();
    return QStringLiteral("BlockZero-Wallet/%1").arg(tag.isEmpty() ? QStringLiteral("dev") : tag);
}
} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

QString UpdateChecker::currentTag()
{
#ifdef BLOZ_RELEASE_TAG
    return QString::fromUtf8(BLOZ_RELEASE_TAG);
#else
    // Fall back to the git tag the build was made from (empty for untagged/dirty builds).
    static const QRegularExpression re(QStringLiteral("^(v\\d+\\.\\d+\\.\\d+(?:-rc\\d+)?)$"));
    const auto m = re.match(QString::fromStdString(FormatFullVersion()));
    return m.hasMatch() ? m.captured(1) : QString();
#endif
}

bool UpdateChecker::isNewer(const QString& candidate, const QString& current)
{
    const Version a = parseTag(candidate);
    const Version b = parseTag(current);
    if (!a.valid || !b.valid) return false;
    auto key = [](const Version& v) {
        // A final release (rc 0) is newer than any of its release candidates.
        return std::make_tuple(v.major, v.minor, v.build, v.rc == 0 ? 1 : 0, v.rc);
    };
    return key(a) > key(b);
}

bool UpdateChecker::startupCheckDue()
{
    QSettings settings;
    if (!settings.value(QStringLiteral("fCheckForUpdates"), true).toBool()) return false;
    const qint64 last = settings.value(QStringLiteral("nLastUpdateCheck"), 0).toLongLong();
    return QDateTime::currentSecsSinceEpoch() - last >= CHECK_INTERVAL_SECS;
}

void UpdateChecker::rememberCheck()
{
    QSettings settings;
    settings.setValue(QStringLiteral("nLastUpdateCheck"), QDateTime::currentSecsSinceEpoch());
}

void UpdateChecker::skipVersion(const QString& tag)
{
    QSettings settings;
    settings.setValue(QStringLiteral("strSkippedUpdate"), tag);
}

bool UpdateChecker::isSkipped(const QString& tag)
{
    QSettings settings;
    return !tag.isEmpty() && settings.value(QStringLiteral("strSkippedUpdate")).toString() == tag;
}

void UpdateChecker::check(bool manual)
{
    if (m_reply) {
        // A check is already running; a manual click just upgrades it to "report everything".
        m_manual = m_manual || manual;
        return;
    }
    m_manual = manual;
    if (currentTag().isEmpty()) {
        if (manual) Q_EMIT failed(tr("This is a development build without a release version, so there is nothing to compare."), manual);
        return;
    }
    QNetworkRequest req{QUrl(QString::fromLatin1(RELEASES_API))};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setTransferTimeout(NETWORK_TIMEOUT_MS);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();
        const bool manual_now = m_manual;
        rememberCheck();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT failed(tr("Could not reach GitHub: %1").arg(reply->errorString()), manual_now);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        Release rel;
        rel.tag = obj.value(QStringLiteral("tag_name")).toString();
        rel.page_url = obj.value(QStringLiteral("html_url")).toString();
        if (rel.page_url.isEmpty()) rel.page_url = QString::fromLatin1(RELEASES_PAGE);
        for (const QJsonValue& a : obj.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = a.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
            if (name == QLatin1String(INSTALLER_NAME)) rel.installer_url = url;
            else if (name == QLatin1String(INSTALLER_NAME) + QLatin1String(".sha256")) rel.checksum_url = url;
        }
        if (!parseTag(rel.tag).valid) {
            Q_EMIT failed(tr("GitHub returned no usable release information."), manual_now);
            return;
        }
        if (isNewer(rel.tag, currentTag())) {
            Q_EMIT updateAvailable(rel, manual_now);
        } else {
            Q_EMIT upToDate(rel.tag, manual_now);
        }
    });
}

namespace {
//! Download url into file (or memory if file is null) with a progress dialog. Returns false when cancelled/failed.
bool download(QNetworkAccessManager& nam, const QUrl& url, QFile* file, QByteArray* memory, QCryptographicHash* hash,
              QProgressDialog& progress, QString* error)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(10 * 60 * 1000);
    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    bool cancelled = false;
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&] {
        const QByteArray chunk = reply->readAll();
        if (file) file->write(chunk);
        if (memory) memory->append(chunk);
        if (hash) hash->addData(chunk);
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [&](qint64 got, qint64 total) {
        if (total > 0) {
            progress.setMaximum(1000);
            progress.setValue(static_cast<int>(got * 1000 / total));
        }
    });
    QObject::connect(&progress, &QProgressDialog::canceled, reply, [&] { cancelled = true; reply->abort(); });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = !cancelled && reply->error() == QNetworkReply::NoError;
    if (!ok && error) *error = cancelled ? QObject::tr("Cancelled.") : reply->errorString();
    reply->deleteLater();
    return ok;
}
} // namespace

void UpdateChecker::downloadAndInstall(const Release& release, QWidget* parent)
{
    if (release.installer_url.isEmpty() || release.checksum_url.isEmpty()) {
        Q_EMIT failed(tr("This release has no Windows installer yet. Please download it from the release page."), true);
        return;
    }
    QProgressDialog progress(tr("Downloading Block Zero %1…").arg(release.tag), tr("Cancel"), 0, 0, parent);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();

    QString error;
    QByteArray checksum_text;
    if (!download(*m_nam, QUrl(release.checksum_url), nullptr, &checksum_text, nullptr, progress, &error)) {
        Q_EMIT failed(tr("Could not download the checksum: %1").arg(error), true);
        return;
    }
    static const QRegularExpression hex64(QStringLiteral("^([0-9a-fA-F]{64})"));
    const auto m = hex64.match(QString::fromLatin1(checksum_text).trimmed());
    if (!m.hasMatch()) {
        Q_EMIT failed(tr("The published checksum could not be read."), true);
        return;
    }
    const QString expected = m.captured(1).toLower();

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    dir.mkpath(QStringLiteral("blockzero-update"));
    const QString path = dir.filePath(QStringLiteral("blockzero-update/blockzero-%1-windows-x64-setup.exe").arg(release.tag));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Q_EMIT failed(tr("Could not write to %1").arg(path), true);
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    progress.setLabelText(tr("Downloading Block Zero %1 installer…").arg(release.tag));
    const bool ok = download(*m_nam, QUrl(release.installer_url), &file, nullptr, &hash, progress, &error);
    file.close();
    if (!ok) {
        QFile::remove(path);
        Q_EMIT failed(tr("Could not download the installer: %1").arg(error), true);
        return;
    }
    const QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
    if (actual != expected) {
        QFile::remove(path);
        Q_EMIT failed(tr("The downloaded installer does not match the published checksum and was deleted. Please try again later."), true);
        return;
    }
    progress.close();

    // Start the installer; the wallet shuts down right after so the installer can replace its files.
    if (!QProcess::startDetached(path, QStringList())) {
        Q_EMIT failed(tr("The installer could not be started. It was saved to %1").arg(path), true);
        return;
    }
    Q_EMIT installerStarted();
}
