#pragma once
#include "filemodel.h"
#include "analyzer.h"
#include "aiclient.h"
#include <QMainWindow>
#include <QTreeView>
#include <QThread>
#include <QProgressDialog>
#include <QLabel>
#include <QListWidget>
#include <QSortFilterProxyModel>

class Scanner;
class SummaryPanel;
class AIChatWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onAddFolder();
    void onRemoveFolder();
    void onScan();
    void onCancelScan();
    void onClear();
    void onSettings();
    void onScanProgress(const QString& path, qint64 files, qint64 size);
    void onScanFinished(FileNode* root, bool cancelled);
    void onScanError(const QString& msg);

private:
    void setupUI();
    void setupMenu();
    void loadSettings();
    void saveSettings();
    void setScanningState(bool scanning);

    // UI components
    QListWidget* m_folderList;
    QTreeView* m_treeView;
    SummaryPanel* m_summaryPanel;
    AIChatWidget* m_chatWidget;
    QProgressDialog* m_progressDialog = nullptr;
    QLabel* m_statusLabel;

    // Actions
    QAction* m_scanAction;
    QAction* m_clearAction;

    // Data
    FileModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    Scanner* m_scanner;
    QThread* m_scanThread;
    AIClient* m_aiClient;
    AnalysisResult m_lastResult;
};
