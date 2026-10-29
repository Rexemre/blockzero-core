// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/paymentserver.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <chainparams.h>
#include <common/args.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <node/interface_ui.h>
#include <policy/policy.h>
#include <wallet/wallet.h>

#include <cstdlib>
#include <memory>

#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFileOpenEvent>
#include <QHash>
#include <QList>
#include <logging.h>

#include <QLocalServer>
#include <QLocalSocket>
#include <QStringList>
#include <QUrlQuery>

const int BITCOIN_IPC_CONNECT_TIMEOUT = 1000; // milliseconds
const QString BITCOIN_IPC_PREFIX("bloz:");

//
// Create a name that is unique for:
//  testnet / non-testnet
//  data directory
//
static QString ipcServerName()
{
    QString name("BitcoinQt");

    // Append a simple hash of the datadir
    // Note that gArgs.GetDataDirNet() returns a different path
    // for -testnet versus main net
    QString ddir(GUIUtil::PathToQString(gArgs.GetDataDirNet()));
    name.append(QString::number(qHash(ddir)));

    return name;
}

//
// We store payment URIs and requests received before
// the main GUI window is up and ready to ask the user
// to send payment.

static QSet<QString> savedPaymentRequests;

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues,
// and the items in savedPaymentRequest will be handled
// when uiReady() is called.
//
// Warning: ipcSendCommandLine() is called early in init,
// so don't use "Q_EMIT message()", but "QMessageBox::"!
//
void PaymentServer::ipcParseCommandLine(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        QString arg(argv[i]);
        if (arg.startsWith("-")) continue;

        if (arg.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive)) // bloz: URI
        {
            savedPaymentRequests.insert(arg);
        }
    }
}

//
// Sending to the server is done synchronously, at startup.
// If the server isn't already running, startup continues,
// and the items in savedPaymentRequest will be handled
// when uiReady() is called.
//
bool PaymentServer::ipcSendCommandLine()
{
    bool fResult = false;
    for (const QString& r : savedPaymentRequests)
    {
        QLocalSocket* socket = new QLocalSocket();
        socket->connectToServer(ipcServerName(), QIODevice::ReadWrite);
        if (!socket->waitForConnected(BITCOIN_IPC_CONNECT_TIMEOUT))
        {
            delete socket;
            socket = nullptr;
            return false;
        }

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);
        out << r;
        out.device()->seek(0);

        socket->write(block);
        socket->flush();
        socket->waitForBytesWritten(BITCOIN_IPC_CONNECT_TIMEOUT);
        // Wait until the running instance confirms it has read the URI (one
        // byte). Closing the pipe right after writing let Windows discard the
        // buffered bytes and left the receiver spinning forever (Qt 6 named
        // pipes), which froze the running wallet.
        socket->waitForReadyRead(BITCOIN_IPC_CONNECT_TIMEOUT);
        socket->readAll();
        socket->disconnectFromServer();
        if (socket->state() != QLocalSocket::UnconnectedState) {
            socket->waitForDisconnected(BITCOIN_IPC_CONNECT_TIMEOUT);
        }

        delete socket;
        socket = nullptr;
        fResult = true;
    }

    return fResult;
}

PaymentServer::PaymentServer(QObject* parent, bool startLocalServer)
    : QObject(parent)
{
    // Install global event filter to catch QFileOpenEvents
    // on Mac: sent when you click bitcoin: links
    // other OSes: helpful when dealing with payment request files
    if (parent)
        parent->installEventFilter(this);

    QString name = ipcServerName();

    // Clean up old socket leftover from a crash:
    QLocalServer::removeServer(name);

    if (startLocalServer)
    {
        uriServer = new QLocalServer(this);

        if (!uriServer->listen(name)) {
            // constructor is called early in init, so don't use "Q_EMIT message()" here
            QMessageBox::critical(nullptr, tr("Payment request error"),
                tr("Cannot start bloz: click-to-pay handler"));
        }
        else {
            connect(uriServer, &QLocalServer::newConnection, this, &PaymentServer::handleURIConnection);
        }
    }
}

PaymentServer::~PaymentServer() = default;

//
// OSX-specific way of handling bloz: URIs
//
bool PaymentServer::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *fileEvent = static_cast<QFileOpenEvent*>(event);
        if (!fileEvent->file().isEmpty())
            handleURIOrFile(fileEvent->file());
        else if (!fileEvent->url().isEmpty())
            handleURIOrFile(fileEvent->url().toString());

        return true;
    }

    return QObject::eventFilter(object, event);
}

void PaymentServer::uiReady()
{
    saveURIs = false;
    for (const QString& s : savedPaymentRequests)
    {
        handleURIOrFile(s);
    }
    savedPaymentRequests.clear();
}

void PaymentServer::handleURIOrFile(const QString& s)
{
    if (saveURIs)
    {
        savedPaymentRequests.insert(s);
        return;
    }

    if (s.startsWith("bitcoin://", Qt::CaseInsensitive))
    {
        Q_EMIT message(tr("URI handling"), tr("'bloz://' is not a valid URI. Use 'bloz:' instead."),
            CClientUIInterface::MSG_ERROR);
    }
    else if (s.startsWith(BITCOIN_IPC_PREFIX, Qt::CaseInsensitive) || s.startsWith("bitcoin:", Qt::CaseInsensitive))
    {
        // bloz:verify?... : a website (bloz.org) asks the user to prove an address by signing a message
        if (AddressProofRequest::isProofURI(s)) {
            AddressProofRequest request;
            QString error;
            if (AddressProofRequest::parse(s, &request, &error)) {
                LogInfo("GUI: address verification request from %s received", request.site.toStdString());
                Q_EMIT receivedAddressProofRequest(request);
            } else {
                LogInfo("GUI: address verification link rejected: %s", error.toStdString());
                Q_EMIT message(tr("Address verification"), error, CClientUIInterface::MSG_ERROR);
            }
            return;
        }
        QUrlQuery uri((QUrl(s)));
        // normal URI
        {
            SendCoinsRecipient recipient;
            if (GUIUtil::parseBitcoinURI(s, &recipient))
            {
                std::string error_msg;
                const CTxDestination dest = DecodeDestination(recipient.address.toStdString(), error_msg);

                if (!IsValidDestination(dest)) {
                    if (uri.hasQueryItem("r")) {  // payment request
                        Q_EMIT message(tr("URI handling"),
                            tr("Cannot process payment request because BIP70 is not supported.\n"
                               "Due to widespread security flaws in BIP70 it's strongly recommended that any merchant instructions to switch wallets be ignored.\n"
                               "If you are receiving this error you should request the merchant provide a BIP21 compatible URI."),
                            CClientUIInterface::ICON_WARNING);
                    }
                    Q_EMIT message(tr("URI handling"), QString::fromStdString(error_msg),
                        CClientUIInterface::MSG_ERROR);
                }
                else
                    Q_EMIT receivedPaymentRequest(recipient);
            }
            else
                Q_EMIT message(tr("URI handling"),
                    tr("URI cannot be parsed! This can be caused by an invalid BLOZ address or malformed URI parameters."),
                    CClientUIInterface::ICON_WARNING);

            return;
        }
    }

    if (QFile::exists(s)) // payment request file
    {
        Q_EMIT message(tr("Payment request file handling"),
            tr("Cannot process payment request because BIP70 is not supported.\n"
               "Due to widespread security flaws in BIP70 it's strongly recommended that any merchant instructions to switch wallets be ignored.\n"
               "If you are receiving this error you should request the merchant provide a BIP21 compatible URI."),
            CClientUIInterface::ICON_WARNING);
    }
}

void PaymentServer::handleURIConnection()
{
    // Read asynchronously: the old blocking "waitForReadyRead until 4 bytes"
    // loop froze the whole GUI on Windows with Qt 6 whenever the sender had
    // already closed its end of the pipe.
    while (QLocalSocket* clientConnection = uriServer->nextPendingConnection()) {
        auto buffer = std::make_shared<QByteArray>();
        auto done = std::make_shared<bool>(false);
        auto tryHandle = [this, clientConnection, buffer, done]() {
            buffer->append(clientConnection->readAll());
            if (*done || buffer->size() < (int)sizeof(quint32)) return;
            QDataStream in(*buffer);
            in.setVersion(QDataStream::Qt_4_0);
            quint32 len = 0;
            in >> len;
            // QDataStream (Qt_4_0) encodes a QString as byte length + UTF-16 data; 0xFFFFFFFF = null string.
            if (len != 0xFFFFFFFF && buffer->size() < (int)(sizeof(quint32) + len)) return;
            *done = true;
            QDataStream in2(*buffer);
            in2.setVersion(QDataStream::Qt_4_0);
            QString msg;
            in2 >> msg;
            // Acknowledge, so the sender may close its end without losing data.
            clientConnection->write("1", 1);
            clientConnection->flush();
            clientConnection->disconnectFromServer();
            if (!msg.isEmpty()) handleURIOrFile(msg);
        };
        connect(clientConnection, &QLocalSocket::readyRead, clientConnection, tryHandle);
        connect(clientConnection, &QLocalSocket::disconnected, clientConnection, [clientConnection, tryHandle] {
            tryHandle();
            clientConnection->deleteLater();
        });
        tryHandle();
    }
}

void PaymentServer::setOptionsModel(OptionsModel *_optionsModel)
{
    this->optionsModel = _optionsModel;
}
