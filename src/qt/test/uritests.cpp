// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uritests.h>

#include <qt/guiutil.h>
#include <qt/updatechecker.h>
#include <qt/walletmodel.h>
#ifdef ENABLE_WALLET
#include <qt/addressproofdialog.h>
#endif

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?req-message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    // Commas in amounts are not allowed.
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // There are two amount specifications. The last value wins.
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=100&amount=200&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.amount == 20000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    // The first amount value is correct. However, the second amount value is not valid. Hence, the URI is not valid.
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=100&amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    // Test label containing a question mark ('?').
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=100&label=?"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("?"));

    // Escape sequences are not supported.
    uri.setUrl(QString("bitcoin:175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W?amount=100&label=%3F"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("%3F"));
}

void URITests::updateVersionTests()
{
    QVERIFY(UpdateChecker::isNewer("v1.0.0-rc40", "v1.0.0-rc39"));
    QVERIFY(UpdateChecker::isNewer("v1.0.0-rc10", "v1.0.0-rc9"));
    QVERIFY(UpdateChecker::isNewer("v1.0.0", "v1.0.0-rc39"));
    QVERIFY(UpdateChecker::isNewer("v1.0.1-rc1", "v1.0.0"));
    // Never a downgrade or a re-install of the same version.
    QVERIFY(!UpdateChecker::isNewer("v1.0.0-rc39", "v1.0.0-rc39"));
    QVERIFY(!UpdateChecker::isNewer("v1.0.0-rc38", "v1.0.0-rc39"));
    QVERIFY(!UpdateChecker::isNewer("v1.0.0-rc39", "v1.0.0"));
    // Unparsable tags are never offered.
    QVERIFY(!UpdateChecker::isNewer("v1.0.0-rc40/../../x", "v1.0.0-rc39"));
    QVERIFY(!UpdateChecker::isNewer("latest", "v1.0.0-rc39"));
    QVERIFY(!UpdateChecker::isNewer("v1.0.0-rc40", ""));
}

void URITests::addressProofTests()
{
#ifdef ENABLE_WALLET
    const QString tail = QStringLiteral("&t=abc&m=Verify%20me&exp=4102444800");
    AddressProofRequest req;
    QVERIFY(AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fbloz.org%2Fcommunity%2Fapi%2Fwallet%2Fconnect%2Fcallback" + tail, &req, nullptr));
    QCOMPARE(req.site, QStringLiteral("bloz.org"));
    QVERIFY(AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fwww.bloz.org%2Fcb" + tail, &req, nullptr));
    // Community subdomains, look-alike hosts, plain http and credentials are refused.
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fevil.projects.bloz.org%2Fcb" + tail, &req, nullptr));
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fbloz.org.evil.com%2Fcb" + tail, &req, nullptr));
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fevilbloz.org%2Fcb" + tail, &req, nullptr));
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=http%3A%2F%2Fbloz.org%2Fcb" + tail, &req, nullptr));
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fa%3Ab%40bloz.org%2Fcb" + tail, &req, nullptr));
    // Expired links are refused.
    QVERIFY(!AddressProofRequest::parse("bloz:verify?v=1&cb=https%3A%2F%2Fbloz.org%2Fcb&t=abc&m=x&exp=1000", &req, nullptr));
#endif
}
