// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ADDRESSPROOFDIALOG_H
#define BITCOIN_QT_ADDRESSPROOFDIALOG_H

#include <QDialog>
#include <QMetaType>
#include <QString>
#include <QUrl>

class PlatformStyle;
class WalletModel;

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

/**
 * A "bloz:verify?..." request. A website (bloz.org) asks the user to prove that
 * one of the wallet's addresses belongs to them: the wallet signs the given
 * message with the chosen address (signmessage) and posts address + signature
 * to the callback URL. No private key ever leaves the wallet, no coins move.
 *
 *   bloz:verify?v=1&site=bloz.org&cb=<https callback>&t=<token>&m=<message>&exp=<unix>
 */
struct AddressProofRequest
{
    QString site;      //!< Human readable requester (always derived from the callback host)
    QUrl callback;     //!< https callback the signature is posted to (host must be allowed)
    QString token;     //!< One-time token issued by the website
    QString message;   //!< Exact text to sign (shown to the user)
    qint64 expires{0}; //!< Unix seconds, 0 = not given

    //! True if the URI is a verification request (bloz:verify?...), valid or not.
    static bool isProofURI(const QString& uri);
    //! Parse and validate. On failure *error holds a user-facing explanation.
    static bool parse(const QString& uri, AddressProofRequest* out, QString* error);
    //! Only bloz.org and www.bloz.org may receive signatures (not community subdomains).
    static bool hostAllowed(const QString& host);
};
Q_DECLARE_METATYPE(AddressProofRequest)

/** Dialog: "bloz.org asks you to verify a Block Zero address". */
class AddressProofDialog : public QDialog
{
    Q_OBJECT

public:
    AddressProofDialog(const PlatformStyle* platformStyle, const AddressProofRequest& request,
                       WalletModel* model, QWidget* parent = nullptr);
    ~AddressProofDialog() override;

private Q_SLOTS:
    void onSign();
    void onNewAddress();
    void onReplyFinished();
    void onCopySignature();

private:
    void fillAddresses(const QString& select = QString());
    void setStatus(const QString& text, bool error);
    void setBusy(bool busy);

    const PlatformStyle* m_platform_style;
    AddressProofRequest m_request;
    WalletModel* m_model;
    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply* m_reply{nullptr};

    QComboBox* m_addresses{nullptr};
    QPushButton* m_new_address{nullptr};
    QPushButton* m_sign{nullptr};
    QPushButton* m_copy_signature{nullptr};
    QPushButton* m_close{nullptr};
    QLabel* m_status{nullptr};
    QString m_signature;
    QString m_signed_address;
};

#endif // BITCOIN_QT_ADDRESSPROOFDIALOG_H
