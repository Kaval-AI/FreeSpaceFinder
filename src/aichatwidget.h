#pragma once
#include "aiclient.h"
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QPair>

class AIChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit AIChatWidget(QWidget* parent = nullptr);

    void setAIClient(AIClient* client);
    void setContext(const QString& context);
    void clearChat();

private slots:
    void onSendClicked();
    void onChunkReceived(const QString& text);
    void onAIFinished(const QString& fullResponse);
    void onAIError(const QString& error);

private:
    void appendMessage(const QString& role, const QString& text);
    void appendUserMessage(const QString& text);
    QLabel* appendAIMessagePlaceholder();
    void appendSystemNote(const QString& text);
    void scrollToBottom();

    AIClient* m_client = nullptr;
    QString m_context;
    QList<QPair<QString, QString>> m_history;  // {role, content}

    QScrollArea* m_scrollArea;
    QWidget* m_messagesWidget;
    QVBoxLayout* m_messagesLayout;
    QLineEdit* m_inputEdit;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;

    QLabel* m_currentAILabel = nullptr;  // label being streamed into
    QString m_currentAIText;
    bool m_streaming = false;
};
