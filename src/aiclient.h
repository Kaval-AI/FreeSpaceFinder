#pragma once
#include <QObject>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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
    void finished(const QString& fullResponse);
    void errorOccurred(const QString& error);

private slots:
    void onReadyRead();
    void onReplyFinished();
    void onNetworkError(QNetworkReply::NetworkError code);

private:
    void processSSEBuffer();

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
    QString m_apiKey;
    QByteArray m_sseBuffer;
    QString m_accumulatedResponse;
};
