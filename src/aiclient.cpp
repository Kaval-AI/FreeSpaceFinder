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
    // Adaptive thinking for deep analysis. display:"summarized" opts into
    // receiving thinking text — the default ("omitted") streams empty blocks.
    QJsonObject thinking;
    thinking["type"] = "adaptive";
    thinking["display"] = "summarized";
    body["thinking"] = thinking;

    QNetworkRequest request{QUrl(QString::fromLatin1(API_URL))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    m_reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_reply, &QNetworkReply::readyRead, this, &AIClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AIClient::onReplyFinished);
}

void AIClient::onReadyRead() {
    if (!m_reply) return;
    m_sseBuffer += m_reply->readAll();
    int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // On HTTP errors the body is a plain JSON error envelope, not an SSE
    // stream — keep buffering and parse it in onReplyFinished instead.
    if (status < 400)
        processSSEBuffer();
}

bool AIClient::processSSEBuffer() {
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
                QString deltaType = delta["type"].toString();
                if (deltaType == "text_delta") {
                    QString text = delta["text"].toString();
                    if (!text.isEmpty()) {
                        m_accumulatedResponse += text;
                        emit chunkReceived(text);
                    }
                } else if (deltaType == "thinking_delta") {
                    QString text = delta["thinking"].toString();
                    if (!text.isEmpty())
                        emit thinkingChunkReceived(text);
                }
            } else if (type == "error") {
                QJsonObject error = obj["error"].toObject();
                AIErrorInfo info;
                info.summary = QString("Anthropic API error (%1): %2")
                                   .arg(error["type"].toString("unknown"))
                                   .arg(error["message"].toString("Unknown API error"));
                if (error["type"].toString() == "overloaded_error")
                    info.hint = "The Anthropic API is temporarily overloaded — try again shortly.";
                emitApiError(info);
                abort();
                return false;
            }
        }
    }
    return true;
}

void AIClient::onReplyFinished() {
    if (!m_reply) return;

    m_sseBuffer += m_reply->readAll();

    const int httpCode =
        m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netError = m_reply->error();
    const QString netErrorDetail = m_reply->errorString();
    const QString requestId = QString::fromLatin1(m_reply->rawHeader("request-id"));
    const QString retryAfter = QString::fromLatin1(m_reply->rawHeader("retry-after"));

    m_reply->deleteLater();
    m_reply = nullptr;

    if (netError == QNetworkReply::OperationCanceledError) {
        m_sseBuffer.clear();
        m_accumulatedResponse.clear();
        return;  // user-initiated abort — not an error
    }

    if (httpCode >= 400) {
        emitApiError(buildHttpError(httpCode, m_sseBuffer, requestId, retryAfter));
        m_sseBuffer.clear();
        m_accumulatedResponse.clear();
        return;
    }

    if (netError != QNetworkReply::NoError) {
        emitApiError(buildNetworkError(netError, netErrorDetail));
        m_sseBuffer.clear();
        m_accumulatedResponse.clear();
        return;
    }

    // Flush any remaining complete SSE lines; bail if an error event was emitted
    if (!processSSEBuffer())
        return;

    emit finished(m_accumulatedResponse);
    m_sseBuffer.clear();
    m_accumulatedResponse.clear();
}

void AIClient::emitApiError(const AIErrorInfo& info) {
    emit errorOccurred(info.fullText());
    emit apiErrorOccurred(info.summary, info.hint, info.detail);
}

AIErrorInfo AIClient::buildHttpError(int httpCode, const QByteArray& body,
                                     const QString& requestId,
                                     const QString& retryAfter) const {
    // The API returns {"type":"error","error":{"type":"...","message":"..."}}
    QString apiType, apiMessage;
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject error = doc.object()["error"].toObject();
        apiType = error["type"].toString();
        apiMessage = error["message"].toString();
    }

    QString hint;
    switch (httpCode) {
    case 400: hint = "The request was invalid."; break;
    case 401: hint = "Authentication failed — check your API key in Settings."; break;
    case 403: hint = "Permission denied — your API key lacks access to this model or feature."; break;
    case 404: hint = "Not found — the requested model may not exist or your key lacks access."; break;
    case 413: hint = "Request too large — try clearing the chat or scanning fewer folders."; break;
    case 429: hint = retryAfter.isEmpty()
                  ? "Rate limited — too many requests. Wait a moment and try again."
                  : QString("Rate limited — retry in %1 seconds.").arg(retryAfter);
              break;
    case 500: hint = "Anthropic server error — try again shortly."; break;
    case 529: hint = "The Anthropic API is temporarily overloaded — try again shortly."; break;
    default:  hint = httpCode >= 500 ? "Server error — try again shortly."
                                     : "Request failed.";
              break;
    }

    AIErrorInfo info;
    info.summary = QString("Anthropic API error (HTTP %1").arg(httpCode);
    if (!apiType.isEmpty())
        info.summary += ", " + apiType;
    info.summary += ")";
    if (!apiMessage.isEmpty())
        info.summary += ": " + apiMessage;
    info.hint = hint;
    if (!requestId.isEmpty())
        info.detail = QString("Request ID: %1").arg(requestId);
    return info;
}

AIErrorInfo AIClient::buildNetworkError(QNetworkReply::NetworkError code,
                                        const QString& detail) const {
    QString hint;
    switch (code) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
        hint = "Cannot reach api.anthropic.com — check your internet connection.";
        break;
    case QNetworkReply::TimeoutError:
        hint = "The request to api.anthropic.com timed out — check your connection and try again.";
        break;
    case QNetworkReply::RemoteHostClosedError:
        hint = "The connection to api.anthropic.com was closed unexpectedly — try again.";
        break;
    case QNetworkReply::SslHandshakeFailedError:
        hint = "Secure connection (SSL/TLS) to api.anthropic.com failed — check your system "
               "certificates, proxy, or firewall.";
        break;
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
        hint = "Proxy error — check your proxy settings.";
        break;
    default:
        hint = "Network error while contacting api.anthropic.com.";
        break;
    }

    AIErrorInfo info;
    info.summary = "Could not reach the Anthropic API.";
    info.hint = hint;
    info.detail = "Details: " + detail;
    return info;
}
