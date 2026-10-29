// Copyright (c) 2026 The Block Zero developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/addressproofdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <addresstype.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <outputtype.h>
#include <common/signmessage.h>
#include <util/result.h>
#include <logging.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {
constexpr int MAX_MESSAGE_LENGTH = 1000;
constexpr int MAX_TOKEN_LENGTH = 200;
constexpr int NETWORK_TIMEOUT_MS = 25000;
} // namespace

// ---------------------------------------------------------------------------
// AddressProofRequest
// ---------------------------------------------------------------------------

bool AddressProofRequest::isProofURI(const QString& uri)
{
    const QString s = uri.trimmed();
    if (!s.startsWith(QLatin1String("bloz:"), Qt::CaseInsensitive)) return false;
    const QUrl url(s);
    if (!url.isValid()) return false;
    QString path = url.path();
    while (path.endsWith('/')) path.chop(1);
    return path.compare(QLatin1String("verify"), Qt::CaseInsensitive) == 0;
}

bool AddressProofRequest::hostAllowed(const QString& host_in)
{
    // Exact hosts only: subdomains such as <slug>.projects.bloz.org are run by
    // community members and must not receive signatures meant for bloz.org.
    const QString host = host_in.toLower();
    return host == QLatin1String("bloz.org") || host == QLatin1String("www.bloz.org");
}

bool AddressProofRequest::parse(const QString& uri, AddressProofRequest* out, QString* error)
{
    auto fail = [&](const QString& why) {
        if (error) *error = why;
        return false;
    };
    if (!isProofURI(uri)) return fail(QObject::tr("This is not a Block Zero verification link."));

    const QUrl url(uri.trimmed());
    const QUrlQuery query(url);
    AddressProofRequest req;

    const QString version = query.queryItemValue(QStringLiteral("v"), QUrl::FullyDecoded);
    if (!version.isEmpty() && version != QLatin1String("1")) {
        return fail(QObject::tr("This verification link needs a newer wallet version (link version %1).").arg(version));
    }

    const QString cb = query.queryItemValue(QStringLiteral("cb"), QUrl::FullyDecoded);
    req.callback = QUrl(cb, QUrl::StrictMode);
    if (cb.isEmpty() || !req.callback.isValid()) return fail(QObject::tr("The verification link has no valid callback address."));
    if (req.callback.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        return fail(QObject::tr("The verification link is not secure (callback is not https) and was ignored."));
    }
    if (!hostAllowed(req.callback.host())) {
        return fail(QObject::tr("Verification requests are only accepted from bloz.org. The link pointed to \"%1\" and was ignored.")
                        .arg(req.callback.host()));
    }
    if (!req.callback.userInfo().isEmpty()) return fail(QObject::tr("The verification link is malformed (credentials in URL)."));
    req.site = req.callback.host().toLower();

    req.token = query.queryItemValue(QStringLiteral("t"), QUrl::FullyDecoded);
    if (req.token.isEmpty() || req.token.size() > MAX_TOKEN_LENGTH) return fail(QObject::tr("The verification link has no valid token."));

    req.message = query.queryItemValue(QStringLiteral("m"), QUrl::FullyDecoded);
    if (req.message.isEmpty() || req.message.size() > MAX_MESSAGE_LENGTH) return fail(QObject::tr("The verification link has no valid message to sign."));
    for (const QChar c : req.message) {
        if (c.category() == QChar::Other_Control) return fail(QObject::tr("The verification message contains control characters and was ignored."));
    }

    const QString exp = query.queryItemValue(QStringLiteral("exp"), QUrl::FullyDecoded);
    if (!exp.isEmpty()) {
        bool ok = false;
        req.expires = exp.toLongLong(&ok);
        if (!ok) req.expires = 0;
    }
    if (req.expires > 0 && req.expires < QDateTime::currentSecsSinceEpoch()) {
        return fail(QObject::tr("This verification link has expired. Go back to %1 and start again.").arg(req.site));
    }

    if (out) *out = req;
    return true;
}

// ---------------------------------------------------------------------------
// AddressProofDialog
// ---------------------------------------------------------------------------

AddressProofDialog::AddressProofDialog(const PlatformStyle* platformStyle, const AddressProofRequest& request,
                                       WalletModel* model, QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags),
      m_platform_style(platformStyle),
      m_request(request),
      m_model(model)
{
    setWindowTitle(tr("Verify address for %1").arg(m_request.site));
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);

    auto* site = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("%1 wants you to confirm that a Block Zero address is yours.").arg(m_request.site.toHtmlEscaped())));
    site->setWordWrap(true);
    layout->addWidget(site);

    auto* hint = new QLabel(tr("Your wallet will sign the message below with the address you pick and send only the address "
                               "and the signature to %1. No private key leaves this computer and no coins move. "
                               "Only continue if you opened this link from %1 yourself.").arg(m_request.site.toHtmlEscaped()));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* msg_label = new QLabel(tr("Message to sign:"));
    layout->addWidget(msg_label);
    auto* msg = new QPlainTextEdit(m_request.message);
    msg->setReadOnly(true);
    msg->setMaximumHeight(90);
    layout->addWidget(msg);

    auto* addr_label = new QLabel(tr("Address to confirm (legacy addresses only, they start with B):"));
    addr_label->setWordWrap(true);
    layout->addWidget(addr_label);
    auto* addr_row = new QHBoxLayout();
    m_addresses = new QComboBox();
    // Long "label (address)" entries must not push the "New address" button out of the dialog.
    m_addresses->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_addresses->setMinimumContentsLength(30);
    m_addresses->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    addr_row->addWidget(m_addresses, 1);
    m_new_address = new QPushButton(tr("New address"));
    m_new_address->setToolTip(tr("Create a fresh legacy address in this wallet for the verification"));
    addr_row->addWidget(m_new_address);
    layout->addLayout(addr_row);

    if (m_request.expires > 0) {
        auto* exp = new QLabel(tr("The link is valid until %1.").arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(m_request.expires), QLocale::ShortFormat)));
        exp->setWordWrap(true);
        layout->addWidget(exp);
    }

    m_status = new QLabel();
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    auto* buttons = new QDialogButtonBox();
    m_sign = buttons->addButton(tr("Sign and send"), QDialogButtonBox::AcceptRole);
    m_sign->setDefault(true);
    m_copy_signature = buttons->addButton(tr("Copy signature"), QDialogButtonBox::ActionRole);
    m_copy_signature->setToolTip(tr("If sending failed: copy the signature and paste it on the website (manual way)."));
    m_copy_signature->setVisible(false);
    m_close = buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(m_sign, &QPushButton::clicked, this, &AddressProofDialog::onSign);
    connect(m_new_address, &QPushButton::clicked, this, &AddressProofDialog::onNewAddress);
    connect(m_copy_signature, &QPushButton::clicked, this, &AddressProofDialog::onCopySignature);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);

    fillAddresses();
    if (m_addresses->count() == 0) {
        setStatus(tr("This wallet has no legacy address yet. Click \"New address\" to create one."), false);
    }
    GUIUtil::handleCloseWindowShortcut(this);
}

AddressProofDialog::~AddressProofDialog()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void AddressProofDialog::fillAddresses(const QString& select)
{
    m_addresses->clear();
    if (!m_model) return;
    AddressTableModel* table = m_model->getAddressTableModel();
    if (!table) return;
    const int rows = table->rowCount(QModelIndex());
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = table->index(row, AddressTableModel::Address, QModelIndex());
        if (table->data(idx, AddressTableModel::TypeRole).toString() != AddressTableModel::Receive) continue;
        const QString address = table->data(idx, Qt::DisplayRole).toString();
        const CTxDestination dest = DecodeDestination(address.toStdString());
        if (!std::get_if<PKHash>(&dest)) continue; // signmessage works with legacy P2PKH addresses only
        const QString label = table->data(table->index(row, AddressTableModel::Label, QModelIndex()), Qt::DisplayRole).toString();
        m_addresses->addItem(label.isEmpty() ? address : QStringLiteral("%1 (%2)").arg(label, address), address);
    }
    if (!select.isEmpty()) {
        const int i = m_addresses->findData(select);
        if (i >= 0) m_addresses->setCurrentIndex(i);
    }
}

void AddressProofDialog::setStatus(const QString& text, bool error)
{
    m_status->setStyleSheet(error ? QStringLiteral("QLabel { color: red; }") : QStringLiteral("QLabel { color: green; }"));
    m_status->setText(text);
}

void AddressProofDialog::setBusy(bool busy)
{
    m_sign->setEnabled(!busy);
    m_new_address->setEnabled(!busy);
    m_addresses->setEnabled(!busy);
}

void AddressProofDialog::onNewAddress()
{
    if (!m_model) return;
    const auto dest = m_model->wallet().getNewDestination(OutputType::LEGACY, m_request.site.toStdString());
    if (!dest) {
        setStatus(tr("Could not create a new address: %1").arg(QString::fromStdString(util::ErrorString(dest).translated)), true);
        return;
    }
    const QString address = QString::fromStdString(EncodeDestination(*dest));
    fillAddresses(address);
    if (m_addresses->findData(address) < 0) {
        // The address table model updates asynchronously; show the new address right away anyway.
        m_addresses->addItem(address, address);
        m_addresses->setCurrentIndex(m_addresses->count() - 1);
    }
    setStatus(tr("New legacy address created. Now click \"Sign and send\"."), false);
}

void AddressProofDialog::onSign()
{
    if (!m_model) return;
    const QString address = m_addresses->currentData().toString();
    if (address.isEmpty()) {
        setStatus(tr("Pick an address first (or create a new one)."), true);
        return;
    }
    if (m_request.expires > 0 && m_request.expires < QDateTime::currentSecsSinceEpoch()) {
        setStatus(tr("This verification link has expired. Go back to %1 and start again.").arg(m_request.site), true);
        return;
    }
    const CTxDestination destination = DecodeDestination(address.toStdString());
    const PKHash* pkhash = std::get_if<PKHash>(&destination);
    if (!pkhash) {
        setStatus(tr("Only legacy addresses (starting with B) can sign messages."), true);
        return;
    }

    WalletModel::UnlockContext ctx(m_model->requestUnlock());
    if (!ctx.isValid()) {
        setStatus(tr("Wallet unlock was cancelled."), true);
        return;
    }

    std::string signature;
    const SigningResult res = m_model->wallet().signMessage(m_request.message.toStdString(), *pkhash, signature);
    if (res != SigningResult::OK) {
        setStatus(res == SigningResult::PRIVATE_KEY_NOT_AVAILABLE ? tr("The private key for this address is not in this wallet.")
                                                                  : tr("Message signing failed."), true);
        return;
    }
    m_signature = QString::fromStdString(signature);
    m_signed_address = address;

    // Post address + signature to the website. Only these two values and the token are sent.
    QJsonObject body;
    body.insert(QStringLiteral("v"), 1);
    body.insert(QStringLiteral("t"), m_request.token);
    body.insert(QStringLiteral("address"), m_signed_address);
    body.insert(QStringLiteral("signature"), m_signature);

    if (!m_nam) m_nam = new QNetworkAccessManager(this);
    QNetworkRequest req(m_request.callback);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BlockZero-Wallet"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("X-BZ", "1");
    req.setTransferTimeout(NETWORK_TIMEOUT_MS);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    setBusy(true);
    setStatus(tr("Signed. Sending the confirmation to %1…").arg(m_request.site), false);
    LogInfo("GUI: address verification signed, posting to %s", m_request.callback.host().toStdString());
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &AddressProofDialog::onReplyFinished);
}

void AddressProofDialog::onReplyFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    setBusy(false);

    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    const bool ok = obj.value(QStringLiteral("ok")).toBool(false);
    LogInfo("GUI: address verification answer from %s: %s (network: %s)", m_request.site.toStdString(),
              ok ? "ok" : "rejected", reply->error() == QNetworkReply::NoError ? "ok" : reply->errorString().toStdString());
    if (ok) {
        const QString text = obj.value(QStringLiteral("message")).toString();
        setStatus(text.isEmpty() ? tr("Done. Your address is now linked on %1.").arg(m_request.site) : text, false);
        m_sign->setVisible(false);
        m_new_address->setEnabled(false);
        m_addresses->setEnabled(false);
        m_close->setText(tr("Close"));
        return;
    }
    QString err = obj.value(QStringLiteral("error")).toString();
    if (err.isEmpty()) {
        err = reply->error() == QNetworkReply::NoError ? tr("Unexpected answer from %1.").arg(m_request.site)
                                                       : tr("Could not reach %1: %2").arg(m_request.site, reply->errorString());
    }
    setStatus(tr("Not linked: %1").arg(err) + QStringLiteral("<br>") +
              tr("You can try again, or copy the signature and paste it on the website (manual way)."), true);
    m_copy_signature->setVisible(true);
}

void AddressProofDialog::onCopySignature()
{
    if (m_signature.isEmpty()) return;
    QApplication::clipboard()->setText(m_signature);
    setStatus(tr("Signature copied. Address: %1").arg(m_signed_address), false);
}
