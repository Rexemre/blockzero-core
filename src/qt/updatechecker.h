// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_UPDATECHECKER_H
#define BITCOIN_QT_UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

/**
 * Checks GitHub Releases of Rexemre/blockzero-core for a newer wallet.
 *
 * - Help > Check for updates… (manual: always reports the result)
 * - at startup, at most once a day, if enabled in Options (reports only when
 *   a newer version exists and was not skipped)
 *
 * On Windows the one-click installer is downloaded, its SHA-256 is compared
 * with the published checksum, and the installer is started. Everywhere else
 * the release page is opened in the browser.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    struct Release {
        QString tag;           //!< e.g. v1.0.0-rc39
        QString page_url;      //!< release page
        QString installer_url; //!< Windows setup.exe (may be empty)
        QString checksum_url;  //!< .sha256 next to the installer (may be empty)
    };

    explicit UpdateChecker(QObject* parent = nullptr);

    //! Tag this binary was released as ("v1.0.0-rc39"); empty for development builds.
    static QString currentTag();
    //! Order of release tags: 1.0.0-rc2 < 1.0.0-rc10 < 1.0.0 < 1.0.1
    static bool isNewer(const QString& candidate, const QString& current);

    //! True if the automatic startup check should run now (enabled + not checked within the last day).
    static bool startupCheckDue();
    static void rememberCheck();
    static void skipVersion(const QString& tag);
    static bool isSkipped(const QString& tag);

public Q_SLOTS:
    //! manual = user asked, so also report "up to date" / errors.
    void check(bool manual);
    //! Download the installer, verify the checksum, start it. Emits installerStarted() on success.
    void downloadAndInstall(const Release& release, QWidget* parent);

Q_SIGNALS:
    void updateAvailable(const UpdateChecker::Release& release, bool manual);
    void upToDate(const QString& tag, bool manual);
    void failed(const QString& error, bool manual);
    void installerStarted();

private:
    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply{nullptr};
    bool m_manual{false};
};

Q_DECLARE_METATYPE(UpdateChecker::Release)

#endif // BITCOIN_QT_UPDATECHECKER_H
