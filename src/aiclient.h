#pragma once
#include <QObject>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Structured description of an API/network failure, for rich error display
struct AIErrorInfo {
    QString summary;  // one line: what happened
    QString hint;     // actionable advice for the user
    QString detail;   // request ID, transport details — for "Show Details..."

    QString fullText() const {
        QString s = summary;
        if (!hint.isEmpty()) s += "\n" + hint;
        if (!detail.isEmpty()) s += "\n" + detail;
        return s;
    }
};

class AIClient : public QObject {
    Q_OBJECT
public:
    explicit AIClient(QObject* parent = nullptr);
    ~AIClient();

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }
    bool isBusy() const { return m_reply != nullptr; }

    // history: list of {role, content} — "user" or "assistant"
    void sendMessage(const QString& systemPrompt,
                     const QList<QPair<QString, QString>>& history,
                     const QString& userMessage);

    void abort();

signals:
    void chunkReceived(const QString& text);
    void thinkingChunkReceived(const QString& text);
    void finished(const QString& fullResponse);
    void errorOccurred(const QString& error);
    // Emitted in addition to errorOccurred for API/network failures (not for
    // local validation errors) — carries the parts for a rich error dialog.
    void apiErrorOccurred(const QString& summary, const QString& hint,
                          const QString& detail);

private slots:
    void onReadyRead();
    void onReplyFinished();

private:
    bool processSSEBuffer();  // false if an SSE error event was emitted
    void emitApiError(const AIErrorInfo& info);
    AIErrorInfo buildHttpError(int httpCode, const QByteArray& body,
                               const QString& requestId,
                               const QString& retryAfter) const;
    AIErrorInfo buildNetworkError(QNetworkReply::NetworkError code,
                                  const QString& detail) const;

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
    QString m_apiKey;
    QByteArray m_sseBuffer;
    QString m_accumulatedResponse;
};
