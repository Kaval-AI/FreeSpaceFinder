/*
FreeSpaceFinder — scans folders and summarizes disk space usage, with an
AI assistant for exploring the results.
Copyright (C) 2026 Kaval.AI

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "aichatwidget.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
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
static const QString thinkingBubbleStyle =
    "background:#f7f6fb; color:#707080; border-radius:12px; padding:8px 12px; "
    "font-size:12px; font-style:italic; margin:2px;";

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
    updateInputState();
}

void AIChatWidget::setAIClient(AIClient* client) {
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
    }
    m_client = client;
    if (m_client) {
        connect(m_client, &AIClient::chunkReceived, this, &AIChatWidget::onChunkReceived);
        connect(m_client, &AIClient::thinkingChunkReceived, this, &AIChatWidget::onThinkingChunkReceived);
        connect(m_client, &AIClient::finished, this, &AIChatWidget::onAIFinished);
        connect(m_client, &AIClient::errorOccurred, this, &AIChatWidget::onAIError);
        connect(m_client, &AIClient::apiErrorOccurred, this, &AIChatWidget::onAIApiError);
    }
}

void AIChatWidget::setContext(const QString& context) {
    m_context = context;
    appendSystemNote("Scan data loaded. You can now ask questions about your disk usage.");
}

void AIChatWidget::setApiKeyAvailable(bool available) {
    if (m_keyAvailable == available) return;
    m_keyAvailable = available;
    updateInputState();
    if (!available)
        appendSystemNote("AI assistant disabled — set your Anthropic API key in Settings.");
}

void AIChatWidget::updateInputState() {
    const bool enabled = m_keyAvailable && !m_streaming;
    m_sendButton->setEnabled(enabled);
    m_inputEdit->setEnabled(enabled);
    m_inputEdit->setPlaceholderText(m_keyAvailable
        ? "Ask about your disk usage..."
        : "Set your Anthropic API key in Settings to enable the AI assistant");
}

void AIChatWidget::clearChat() {
    m_history.clear();

    // Remove all message widgets except the stretch at the bottom
    while (m_messagesLayout->count() > 1) {
        QLayoutItem* item = m_messagesLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Pointers above were deleted with their rows — drop any in-flight stream state
    m_currentAILabel = nullptr;
    m_currentAIRow = nullptr;
    m_currentAIText.clear();
    m_currentThinkingLabel = nullptr;
    m_currentThinkingText.clear();

    appendSystemNote("Chat cleared.");
}

void AIChatWidget::onSendClicked() {
    if (!m_client) return;
    if (m_streaming || !m_keyAvailable) return;

    QString text = m_inputEdit->text().trimmed();
    if (text.isEmpty()) return;

    if (m_context.isEmpty()) {
        appendSystemNote("No scan data available. Please scan a folder first.");
        return;
    }

    m_inputEdit->clear();
    appendUserMessage(text);

    m_streaming = true;
    updateInputState();
    m_currentAILabel = appendAIMessagePlaceholder();
    m_currentAIText.clear();
    m_currentThinkingLabel = nullptr;
    m_currentThinkingText.clear();

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

void AIChatWidget::onThinkingChunkReceived(const QString& text) {
    if (!m_currentAIRow) return;

    if (!m_currentThinkingLabel) {
        // Insert a thinking bubble just above the (pending) answer bubble
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        m_currentThinkingLabel = new QLabel;
        m_currentThinkingLabel->setStyleSheet(thinkingBubbleStyle);
        m_currentThinkingLabel->setWordWrap(true);
        m_currentThinkingLabel->setMaximumWidth(560);
        m_currentThinkingLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
        m_currentThinkingLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rowLayout->addWidget(m_currentThinkingLabel);
        rowLayout->addStretch();

        int idx = m_messagesLayout->indexOf(m_currentAIRow);
        m_messagesLayout->insertWidget(idx >= 0 ? idx : m_messagesLayout->count() - 1, row);
    }

    m_currentThinkingText += text;
    m_currentThinkingLabel->setText("💭 " + m_currentThinkingText);
    scrollToBottom();
}

void AIChatWidget::onAIFinished(const QString& fullResponse) {
    m_streaming = false;
    updateInputState();
    m_inputEdit->setFocus();

    if (!fullResponse.isEmpty())
        m_history.append({"assistant", fullResponse});

    m_currentAILabel = nullptr;
    m_currentAIRow = nullptr;
    m_currentAIText.clear();
    m_currentThinkingLabel = nullptr;
    m_currentThinkingText.clear();
    scrollToBottom();
}

void AIChatWidget::onAIError(const QString& error) {
    m_streaming = false;
    updateInputState();
    m_currentAILabel = nullptr;
    m_currentAIRow = nullptr;
    m_currentAIText.clear();
    m_currentThinkingLabel = nullptr;
    m_currentThinkingText.clear();
    // Inline transcript note shows just the first line; API/network errors
    // additionally get a detailed dialog via onAIApiError().
    appendSystemNote("Error: " + error.section('\n', 0, 0));
}

void AIChatWidget::onAIApiError(const QString& summary, const QString& hint,
                                const QString& detail) {
    // errorOccurred (state reset + inline note) is always emitted before this
    QMessageBox box(QMessageBox::Critical, "AI Assistant Error",
                    summary, QMessageBox::Ok, this);
    if (!hint.isEmpty())
        box.setInformativeText(hint);
    if (!detail.isEmpty())
        box.setDetailedText(detail);  // collapsed behind "Show Details..."
    box.exec();
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
    m_currentAIRow = row;
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
