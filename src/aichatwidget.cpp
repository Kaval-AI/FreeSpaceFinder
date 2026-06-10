#include "aichatwidget.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollBar>
#include <QTimer>
#include <QSizePolicy>

static const QString userBubbleStyle =
    "background:#2060c0; color:white; border-radius:12px; padding:8px 12px; "
    "font-size:13px; margin:2px;";
static const QString aiBubbleStyle =
    "background:#f0f0f0; color:#202020; border-radius:12px; padding:8px 12px; "
    "font-size:13px; margin:2px;";
static const QString systemNoteStyle =
    "color:#808080; font-style:italic; font-size:11px; padding:2px 8px;";

AIChatWidget::AIChatWidget(QWidget* parent) : QWidget(parent) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header
    auto* header = new QLabel("AI Assistant (Claude)");
    header->setStyleSheet("font-weight:bold; font-size:13px; "
                          "background:#1a1a2e; color:white; padding:6px 12px;");
    rootLayout->addWidget(header);

    // Scroll area for messages
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_messagesWidget = new QWidget;
    m_messagesLayout = new QVBoxLayout(m_messagesWidget);
    m_messagesLayout->setContentsMargins(8, 8, 8, 8);
    m_messagesLayout->setSpacing(6);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesWidget);
    rootLayout->addWidget(m_scrollArea, 1);

    // Input area
    auto* inputFrame = new QFrame;
    inputFrame->setFrameShape(QFrame::StyledPanel);
    inputFrame->setStyleSheet("background:#fafafa; border-top:1px solid #ddd;");
    auto* inputLayout = new QHBoxLayout(inputFrame);
    inputLayout->setContentsMargins(8, 6, 8, 6);

    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText("Ask about your disk usage...");
    m_inputEdit->setStyleSheet("border:1px solid #ccc; border-radius:4px; padding:4px 8px; font-size:13px;");

    m_sendButton = new QPushButton("Send");
    m_sendButton->setStyleSheet("background:#2060c0; color:white; border-radius:4px; "
                                "padding:4px 16px; font-size:13px;");
    m_sendButton->setDefault(true);

    m_clearButton = new QPushButton("Clear");
    m_clearButton->setStyleSheet("background:#e0e0e0; border-radius:4px; "
                                 "padding:4px 12px; font-size:13px;");

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addWidget(m_sendButton);
    inputLayout->addWidget(m_clearButton);
    rootLayout->addWidget(inputFrame);

    connect(m_inputEdit, &QLineEdit::returnPressed, this, &AIChatWidget::onSendClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &AIChatWidget::onSendClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &AIChatWidget::clearChat);

    appendSystemNote("Scan folders and click Scan to load data. Then ask questions about your disk usage.");
}

void AIChatWidget::setAIClient(AIClient* client) {
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
    }
    m_client = client;
    if (m_client) {
        connect(m_client, &AIClient::chunkReceived, this, &AIChatWidget::onChunkReceived);
        connect(m_client, &AIClient::finished, this, &AIChatWidget::onAIFinished);
        connect(m_client, &AIClient::errorOccurred, this, &AIChatWidget::onAIError);
    }
}

void AIChatWidget::setContext(const QString& context) {
    m_context = context;
    appendSystemNote("Scan data loaded. You can now ask questions about your disk usage.");
}

void AIChatWidget::clearChat() {
    m_history.clear();

    // Remove all message widgets except the stretch at the bottom
    while (m_messagesLayout->count() > 1) {
        QLayoutItem* item = m_messagesLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    appendSystemNote("Chat cleared.");
}

void AIChatWidget::onSendClicked() {
    if (!m_client) return;
    if (m_streaming) return;

    QString text = m_inputEdit->text().trimmed();
    if (text.isEmpty()) return;

    if (m_context.isEmpty()) {
        appendSystemNote("No scan data available. Please scan a folder first.");
        return;
    }

    m_inputEdit->clear();
    appendUserMessage(text);

    m_sendButton->setEnabled(false);
    m_inputEdit->setEnabled(false);
    m_streaming = true;
    m_currentAILabel = appendAIMessagePlaceholder();
    m_currentAIText.clear();

    m_client->sendMessage(m_context, m_history, text);
    // Add to history immediately (response will be added on finish)
    m_history.append({"user", text});
}

void AIChatWidget::onChunkReceived(const QString& text) {
    if (!m_currentAILabel) return;
    m_currentAIText += text;
    m_currentAILabel->setText(m_currentAIText);
    scrollToBottom();
}

void AIChatWidget::onAIFinished(const QString& fullResponse) {
    m_streaming = false;
    m_sendButton->setEnabled(true);
    m_inputEdit->setEnabled(true);
    m_inputEdit->setFocus();

    if (!fullResponse.isEmpty())
        m_history.append({"assistant", fullResponse});

    m_currentAILabel = nullptr;
    m_currentAIText.clear();
    scrollToBottom();
}

void AIChatWidget::onAIError(const QString& error) {
    m_streaming = false;
    m_sendButton->setEnabled(true);
    m_inputEdit->setEnabled(true);
    m_currentAILabel = nullptr;
    m_currentAIText.clear();
    appendSystemNote("Error: " + error);
}

void AIChatWidget::appendUserMessage(const QString& text) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch();

    auto* bubble = new QLabel(text.toHtmlEscaped());
    bubble->setStyleSheet(userBubbleStyle);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(500);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
    rowLayout->addWidget(bubble);

    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, row);
    scrollToBottom();
}

QLabel* AIChatWidget::appendAIMessagePlaceholder() {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto* bubble = new QLabel("...");
    bubble->setStyleSheet(aiBubbleStyle);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(560);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rowLayout->addWidget(bubble);
    rowLayout->addStretch();

    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, row);
    scrollToBottom();
    return bubble;
}

void AIChatWidget::appendSystemNote(const QString& text) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch();

    auto* label = new QLabel(text);
    label->setStyleSheet(systemNoteStyle);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    rowLayout->addWidget(label);
    rowLayout->addStretch();

    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, row);
    scrollToBottom();
}

void AIChatWidget::scrollToBottom() {
    QTimer::singleShot(0, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}
