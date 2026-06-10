#include <QtTest>
#include <QSignalSpy>
#include "aiclient.h"

// Tests for AIClient. The tests that talk to the live Anthropic endpoint are
// skipped unless ANTHROPIC_API_KEY is set in the environment, so CI without a
// key (and offline runs) stay green.

class TestAIClient : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void missingKeyEmitsError();
    void busyClientRejectsSecondRequest();
    void liveCompletion();
    void liveCompletionEmitsNoError();
    void liveInvalidKeyReportsAuthError();

private:
    QString m_apiKey;
};

void TestAIClient::initTestCase() {
    m_apiKey = qEnvironmentVariable("ANTHROPIC_API_KEY");
    if (m_apiKey.isEmpty())
        qInfo("ANTHROPIC_API_KEY not set — live endpoint tests will be skipped.");
}

void TestAIClient::missingKeyEmitsError() {
    AIClient client;
    QSignalSpy errorSpy(&client, &AIClient::errorOccurred);
    QSignalSpy finishedSpy(&client, &AIClient::finished);

    client.sendMessage("system", {}, "hello");

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().first().toString().contains("API key"));
    QCOMPARE(finishedSpy.count(), 0);
}

void TestAIClient::busyClientRejectsSecondRequest() {
    if (m_apiKey.isEmpty())
        QSKIP("ANTHROPIC_API_KEY not set");

    AIClient client;
    client.setApiKey(m_apiKey);
    QSignalSpy errorSpy(&client, &AIClient::errorOccurred);
    QSignalSpy finishedSpy(&client, &AIClient::finished);

    client.sendMessage("Reply with one word.", {}, "ping");
    QVERIFY(client.isBusy());
    client.sendMessage("Reply with one word.", {}, "ping again");
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.first().first().toString().contains("Already processing"));

    client.abort();
    QVERIFY(!client.isBusy());
}

void TestAIClient::liveCompletion() {
    if (m_apiKey.isEmpty())
        QSKIP("ANTHROPIC_API_KEY not set");

    AIClient client;
    client.setApiKey(m_apiKey);
    QSignalSpy finishedSpy(&client, &AIClient::finished);
    QSignalSpy errorSpy(&client, &AIClient::errorOccurred);
    QSignalSpy chunkSpy(&client, &AIClient::chunkReceived);

    client.sendMessage(
        "You are a connectivity test. Reply with exactly the word PONG and nothing else.",
        {}, "ping");

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1 || errorSpy.count() == 1, 60000);
    if (errorSpy.count() > 0)
        QFAIL(qPrintable("AIClient reported an error: " +
                         errorSpy.first().first().toString()));

    QVERIFY(chunkSpy.count() >= 1);
    const QString response = finishedSpy.first().first().toString();
    QVERIFY2(response.contains("PONG", Qt::CaseInsensitive),
             qPrintable("Unexpected response: " + response));
    QVERIFY(!client.isBusy());
}

void TestAIClient::liveCompletionEmitsNoError() {
    if (m_apiKey.isEmpty())
        QSKIP("ANTHROPIC_API_KEY not set");

    // Multi-turn history round-trip against the live endpoint
    AIClient client;
    client.setApiKey(m_apiKey);
    QSignalSpy finishedSpy(&client, &AIClient::finished);
    QSignalSpy errorSpy(&client, &AIClient::errorOccurred);

    QList<QPair<QString, QString>> history;
    history.append(qMakePair(QString("user"),
                             QString("My favorite color is teal. Acknowledge with OK.")));
    history.append(qMakePair(QString("assistant"), QString("OK.")));

    client.sendMessage("Answer in one short sentence.", history,
                       "What is my favorite color?");

    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() == 1 || errorSpy.count() == 1, 60000);
    if (errorSpy.count() > 0)
        QFAIL(qPrintable("AIClient reported an error: " +
                         errorSpy.first().first().toString()));

    const QString response = finishedSpy.first().first().toString();
    QVERIFY2(response.contains("teal", Qt::CaseInsensitive),
             qPrintable("History was not used. Response: " + response));
}

void TestAIClient::liveInvalidKeyReportsAuthError() {
    if (m_apiKey.isEmpty())
        QSKIP("ANTHROPIC_API_KEY not set (test needs network access)");

    AIClient client;
    client.setApiKey("sk-ant-invalid-key-for-testing");
    QSignalSpy errorSpy(&client, &AIClient::errorOccurred);
    QSignalSpy apiErrorSpy(&client, &AIClient::apiErrorOccurred);
    QSignalSpy finishedSpy(&client, &AIClient::finished);

    client.sendMessage("test", {}, "ping");

    QTRY_VERIFY_WITH_TIMEOUT(errorSpy.count() == 1, 60000);
    const QString error = errorSpy.first().first().toString();
    QVERIFY2(error.contains("401") || error.contains("authentication", Qt::CaseInsensitive),
             qPrintable("Expected an authentication error, got: " + error));
    QVERIFY2(error.contains("API key in Settings"),
             qPrintable("Expected the actionable hint, got: " + error));
    QCOMPARE(finishedSpy.count(), 0);

    // The structured signal fires alongside, with summary/hint split out
    QCOMPARE(apiErrorSpy.count(), 1);
    const QList<QVariant> parts = apiErrorSpy.first();
    QVERIFY2(parts.at(0).toString().contains("401"),
             qPrintable("Expected 401 in summary, got: " + parts.at(0).toString()));
    QVERIFY2(parts.at(1).toString().contains("API key in Settings"),
             qPrintable("Expected hint, got: " + parts.at(1).toString()));
}

QTEST_GUILESS_MAIN(TestAIClient)
#include "tst_aiclient.moc"
