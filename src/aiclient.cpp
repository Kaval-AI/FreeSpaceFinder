#include "aiclient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

static const char* API_URL = "https://api.anthropic.com/v1/messages";
static const char* MODEL   = "claude-opus-4-8";

AIClient::AIClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

AIClient::~AIClient() {
    abort();
}

void AIClient::setApiKey(const QString& key) {
    m_apiKey = key.trimmed();
}

void AIClient::abort() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_sseBuffer.clear();
    m_accumulatedResponse.clear();
}

void AIClient::sendMessage(const QString& systemPrompt,
                           const QList<QPair<QString, QString>>& history,
                           const QString& userMessage) {
    if (m_apiKey.isEmpty()) {
        emit errorOccurred("API key is not set. Enter your Anthropic API key in Settings.");
        return;
    }
    if (m_reply) {
        emit errorOccurred("Already processing a request.");
        return;
    }

    m_sseBuffer.clear();
    m_accumulatedResponse.clear();

    // Build messages array
    QJsonArray messages;
    for (const auto& [role, content] : history) {
        QJsonObject msg;
        msg["role"] = role;
        msg["content"] = content;
        messages.append(msg);
    }
    // Append the new user message
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = MODEL;
    body["max_tokens"] = 4096;
    body["stream"] = true;
    body["system"] = systemPrompt;
    body["messages"] = messages;
    // Adaptive thinking for deep analysis
    QJsonObject thinking;
    thinking["type"] = "adaptive";
    body["thinking"] = thinking;

    QNetworkRequest request{QUrl(QString::fromLatin1(API_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    m_reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_reply, &QNetworkReply::readyRead, this, &AIClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AIClient::onReplyFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &AIClient::onNetworkError);
}

void AIClient::onReadyRead() {
    if (!m_reply) return;
    m_sseBuffer += m_reply->readAll();
    processSSEBuffer();
}

void AIClient::processSSEBuffer() {
    while (true) {
        int newline = m_sseBuffer.indexOf('\n');
        if (newline < 0) break;

        QByteArray line = m_sseBuffer.left(newline).trimmed();
        m_sseBuffer.remove(0, newline + 1);

        if (line.isEmpty() || line.startsWith(':')) continue;  // SSE comment or keep-alive

        if (line.startsWith("data: ")) {
            QByteArray jsonData = line.mid(6).trimmed();
            if (jsonData == "[DONE]") continue;

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
            if (err.error != QJsonParseError::NoError) continue;

            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();

            if (type == "content_block_delta") {
                QJsonObject delta = obj["delta"].toObject();
                if (delta["type"].toString() == "text_delta") {
                    QString text = delta["text"].toString();
                    if (!text.isEmpty()) {
                        m_accumulatedResponse += text;
                        emit chunkReceived(text);
                    }
                }
            } else if (type == "error") {
                QJsonObject error = obj["error"].toObject();
                QString msg = error["message"].toString("Unknown API error");
                emit errorOccurred(msg);
                abort();
                return;
            }
        }
    }
}

void AIClient::onReplyFinished() {
    if (!m_reply) return;

    // Flush any remaining buffer
    m_sseBuffer += m_reply->readAll();
    processSSEBuffer();

    int httpCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    m_reply->deleteLater();
    m_reply = nullptr;

    if (httpCode >= 400) {
        emit errorOccurred(QString("HTTP error %1. Check your API key.").arg(httpCode));
        return;
    }

    emit finished(m_accumulatedResponse);
    m_accumulatedResponse.clear();
}

void AIClient::onNetworkError(QNetworkReply::NetworkError code) {
    QString msg = m_reply ? m_reply->errorString() : QString("Network error %1").arg(code);
    emit errorOccurred(msg);
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_sseBuffer.clear();
    m_accumulatedResponse.clear();
}
