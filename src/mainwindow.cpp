#include "mainwindow.h"
#include "scanner.h"
#include "summarypanel.h"
#include "aichatwidget.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOption>
#include <QLabel>
#include <QAction>

// Custom delegate to draw size bars in the tree view
class SizeBarDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (index.column() != FileModel::ColSize) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyledItemDelegate::paint(painter, option, index);

        qint64 nodeSize = index.data(FileModel::SizeRole).toLongLong();
        qint64 maxSize  = index.data(FileModel::MaxSizeRole).toLongLong();
        if (maxSize <= 0 || nodeSize <= 0) return;

        double ratio = static_cast<double>(nodeSize) / maxSize;
        ratio = qMin(ratio, 1.0);

        QRect barRect = option.rect;
        barRect.setWidth(static_cast<int>(barRect.width() * ratio));
        barRect.adjust(0, barRect.height() - 4, 0, 0);
        barRect.setHeight(3);

        bool isDir = index.data(FileModel::IsDirectoryRole).toBool();
        QColor color = isDir ? QColor(60, 120, 220, 160) : QColor(220, 80, 60, 160);
        painter->fillRect(barRect, color);
    }
};

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FreeSpaceFinder");
    setMinimumSize(1000, 700);
    resize(1280, 800);

    m_aiClient = new AIClient(this);
    m_model = new FileModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortRole(FileModel::SizeRole);

    m_scanner = new Scanner;
    m_scanThread = new QThread(this);
    m_scanner->moveToThread(m_scanThread);

    connect(m_scanThread, &QThread::finished, m_scanner, &QObject::deleteLater);
    connect(m_scanner, &Scanner::progress, this, &MainWindow::onScanProgress,
            Qt::QueuedConnection);
    connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished,
            Qt::QueuedConnection);
    connect(m_scanner, &Scanner::errorOccurred, this, &MainWindow::onScanError,
            Qt::QueuedConnection);

    m_scanThread->start();

    setupUI();
    setupMenu();
    loadSettings();

    m_summaryPanel->clear();
}

MainWindow::~MainWindow() {
    saveSettings();
    m_scanner->cancel();
    m_scanThread->quit();
    m_scanThread->wait(3000);
}

void MainWindow::setupUI() {
    // --- Toolbar ---
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto* addFolderAction = toolbar->addAction("Add Folder");
    addFolderAction->setToolTip("Add a folder to scan");
    connect(addFolderAction, &QAction::triggered, this, &MainWindow::onAddFolder);

    auto* removeFolderAction = toolbar->addAction("Remove");
    removeFolderAction->setToolTip("Remove selected folder from list");
    connect(removeFolderAction, &QAction::triggered, this, &MainWindow::onRemoveFolder);

    toolbar->addSeparator();

    m_scanAction = toolbar->addAction("Scan");
    m_scanAction->setShortcut(QKeySequence("Ctrl+R"));
    m_scanAction->setToolTip("Start scanning (Ctrl+R)");
    connect(m_scanAction, &QAction::triggered, this, &MainWindow::onScan);

    m_cancelAction = toolbar->addAction("Cancel");
    m_cancelAction->setEnabled(false);
    connect(m_cancelAction, &QAction::triggered, m_scanner, &Scanner::cancel);

    m_clearAction = toolbar->addAction("Clear");
    connect(m_clearAction, &QAction::triggered, this, &MainWindow::onClear);

    toolbar->addSeparator();

    auto* settingsAction = toolbar->addAction("Settings");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

    // Progress
    toolbar->addSeparator();
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setMinimumWidth(200);
    toolbar->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedWidth(160);
    toolbar->addWidget(m_progressBar);

    // --- Central widget ---
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top: folder list + tree + summary
    auto* topSplitter = new QSplitter(Qt::Horizontal);

    // Left pane: folder list + tree
    auto* leftWidget = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(4);

    auto* folderLabel = new QLabel("Folders to scan:");
    folderLabel->setStyleSheet("font-weight:bold;");
    leftLayout->addWidget(folderLabel);

    m_folderList = new QListWidget;
    m_folderList->setMaximumHeight(90);
    m_folderList->setToolTip("Folders to scan. Double-click to remove.");
    connect(m_folderList, &QListWidget::itemDoubleClicked, this, [this]() {
        onRemoveFolder();
    });
    leftLayout->addWidget(m_folderList);

    m_treeView = new QTreeView;
    m_treeView->setModel(m_proxyModel);
    m_treeView->setItemDelegateForColumn(FileModel::ColSize, new SizeBarDelegate(this));
    m_treeView->header()->setSectionResizeMode(FileModel::ColName, QHeaderView::Stretch);
    m_treeView->header()->setSectionResizeMode(FileModel::ColSize, QHeaderView::Interactive);
    m_treeView->header()->setSectionResizeMode(FileModel::ColLastAccessed, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(FileModel::ColModified, QHeaderView::ResizeToContents);
    m_treeView->header()->resizeSection(FileModel::ColSize, 160);
    m_treeView->setSortingEnabled(true);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    leftLayout->addWidget(m_treeView, 1);

    topSplitter->addWidget(leftWidget);

    // Right pane: summary panel
    m_summaryPanel = new SummaryPanel;
    topSplitter->addWidget(m_summaryPanel);
    topSplitter->setStretchFactor(0, 4);
    topSplitter->setStretchFactor(1, 5);

    // Main vertical split: top / chat
    auto* vertSplitter = new QSplitter(Qt::Vertical);
    vertSplitter->addWidget(topSplitter);

    m_chatWidget = new AIChatWidget;
    m_chatWidget->setAIClient(m_aiClient);
    m_chatWidget->setMinimumHeight(180);
    vertSplitter->addWidget(m_chatWidget);
    vertSplitter->setStretchFactor(0, 3);
    vertSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(vertSplitter);
    statusBar()->showMessage("Ready — add folders and click Scan");
}

void MainWindow::setupMenu() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Add Folder...", this, &MainWindow::onAddFolder, QKeySequence("Ctrl+O"));
    fileMenu->addAction("&Scan", this, &MainWindow::onScan, QKeySequence("Ctrl+R"));
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", qApp, &QApplication::quit, QKeySequence("Ctrl+Q"));
}

void MainWindow::loadSettings() {
    QSettings s("FreeSpaceFinder", "FreeSpaceFinder");
    QStringList folders = s.value("folders").toStringList();
    for (const QString& f : folders) {
        if (!f.isEmpty())
            m_folderList->addItem(f);
    }
    QString apiKey = s.value("apiKey").toString();
    if (!apiKey.isEmpty())
        m_aiClient->setApiKey(apiKey);
    else {
        // Fall back to env var
        QString envKey = qEnvironmentVariable("ANTHROPIC_API_KEY");
        if (!envKey.isEmpty())
            m_aiClient->setApiKey(envKey);
    }
    restoreGeometry(s.value("geometry").toByteArray());
}

void MainWindow::saveSettings() {
    QSettings s("FreeSpaceFinder", "FreeSpaceFinder");
    QStringList folders;
    for (int i = 0; i < m_folderList->count(); ++i)
        folders << m_folderList->item(i)->text();
    s.setValue("folders", folders);
    s.setValue("apiKey", m_aiClient->apiKey());
    s.setValue("geometry", saveGeometry());
}

void MainWindow::onAddFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select folder to scan",
                                                    QDir::homePath());
    if (dir.isEmpty()) return;
    // Avoid duplicates
    for (int i = 0; i < m_folderList->count(); ++i) {
        if (m_folderList->item(i)->text() == dir) return;
    }
    m_folderList->addItem(dir);
}

void MainWindow::onRemoveFolder() {
    QList<QListWidgetItem*> selected = m_folderList->selectedItems();
    for (auto* item : selected)
        delete item;
}

void MainWindow::onScan() {
    if (m_folderList->count() == 0) {
        QMessageBox::information(this, "No Folders",
            "Add at least one folder to scan using the 'Add Folder' button.");
        return;
    }

    QStringList paths;
    for (int i = 0; i < m_folderList->count(); ++i)
        paths << m_folderList->item(i)->text();

    m_model->clear();
    m_summaryPanel->clear();
    m_chatWidget->clearChat();

    setScanningState(true);
    QMetaObject::invokeMethod(m_scanner, "scan", Qt::QueuedConnection,
                              Q_ARG(QStringList, paths));
}

void MainWindow::onCancelScan() {
    m_scanner->cancel();
    setScanningState(false);
    statusBar()->showMessage("Scan cancelled.");
}

void MainWindow::onClear() {
    m_model->clear();
    m_summaryPanel->clear();
    m_chatWidget->clearChat();
    m_lastResult = AnalysisResult{};
    statusBar()->showMessage("Cleared.");
}

void MainWindow::onSettings() {
    bool ok;
    QString key = QInputDialog::getText(
        this, "API Key", "Enter Anthropic API key:",
        QLineEdit::Password, m_aiClient->apiKey(), &ok);
    if (ok)
        m_aiClient->setApiKey(key);
}

void MainWindow::onScanProgress(const QString& path, qint64 files, qint64 size) {
    QString shortenedPath = path.length() > 60 ? "..." + path.right(57) : path;
    m_statusLabel->setText(QString("Scanning... %1 files (%2)")
                               .arg(files).arg(FileNode::formatSize(size)));
    statusBar()->showMessage("Scanning: " + shortenedPath);
}

void MainWindow::onScanFinished(FileNode* root) {
    setScanningState(false);

    QStringList paths;
    for (int i = 0; i < m_folderList->count(); ++i)
        paths << m_folderList->item(i)->text();

    Analyzer analyzer;
    m_lastResult = analyzer.analyze(root, paths);

    m_model->setRoot(std::unique_ptr<FileNode>(root));

    // Expand first level
    m_treeView->expandToDepth(0);
    m_treeView->sortByColumn(FileModel::ColSize, Qt::DescendingOrder);

    m_summaryPanel->setResult(m_lastResult);
    m_chatWidget->setContext(m_lastResult.aiContext);

    statusBar()->showMessage(
        QString("Scan complete — %1 files, %2 dirs, %3 total")
            .arg(m_lastResult.totalFiles)
            .arg(m_lastResult.totalDirs)
            .arg(FileNode::formatSize(m_lastResult.totalSize)));
    m_statusLabel->setText(FileNode::formatSize(m_lastResult.totalSize) +
                           " in " + QString::number(m_lastResult.totalFiles) + " files");
}

void MainWindow::onScanError(const QString& msg) {
    statusBar()->showMessage("Error: " + msg);
    QMessageBox::warning(this, "Scan Error", msg);
}

void MainWindow::setScanningState(bool scanning) {
    m_scanAction->setEnabled(!scanning);
    m_cancelAction->setEnabled(scanning);
    m_clearAction->setEnabled(!scanning);
    m_progressBar->setVisible(scanning);
    if (!scanning) m_statusLabel->setText("Ready");
}
