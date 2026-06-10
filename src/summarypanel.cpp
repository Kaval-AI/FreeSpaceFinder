#include "summarypanel.h"
#include <QVBoxLayout>
#include <QHeaderView>

SummaryPanel::SummaryPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    // Overview tab
    auto* overviewWidget = new QWidget;
    auto* overviewLayout = new QVBoxLayout(overviewWidget);
    m_overviewLabel = new QLabel;
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_overviewLabel->setTextFormat(Qt::RichText);
    m_overviewLabel->setContentsMargins(12, 12, 12, 12);
    overviewLayout->addWidget(m_overviewLabel);
    overviewLayout->addStretch();
    m_tabs->addTab(overviewWidget, "Overview");

    // Helper to make a table
    auto makeTable = [](QStringList headers) -> QTableWidget* {
        auto* t = new QTableWidget;
        t->setColumnCount(headers.size());
        t->setHorizontalHeaderLabels(headers);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setAlternatingRowColors(true);
        t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < headers.size(); ++i)
            t->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        t->verticalHeader()->setVisible(false);
        t->setSortingEnabled(true);
        return t;
    };

    m_largestFilesTable = makeTable({"Path", "Size", "Last Accessed", "Modified", "Type"});
    m_tabs->addTab(m_largestFilesTable, "Largest Files");

    m_largestDirsTable = makeTable({"Path", "Size", "Files"});
    m_tabs->addTab(m_largestDirsTable, "Largest Dirs");

    m_byTypeTable = makeTable({"Extension", "Total Size", "File Count", "% of Total"});
    m_byTypeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_byTypeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tabs->addTab(m_byTypeTable, "By Type");

    m_oldFilesTable = makeTable({"Path", "Size", "Last Accessed", "Modified", "Type"});
    m_tabs->addTab(m_oldFilesTable, "Old Files (>1yr)");

    m_suggestionsView = new QTextBrowser;
    m_suggestionsView->setContentsMargins(12, 12, 12, 12);
    m_tabs->addTab(m_suggestionsView, "Suggestions");
}

void SummaryPanel::clear() {
    m_overviewLabel->setText("No scan performed yet.\n\nAdd folders and click Scan.");
    m_largestFilesTable->setRowCount(0);
    m_largestDirsTable->setRowCount(0);
    m_byTypeTable->setRowCount(0);
    m_oldFilesTable->setRowCount(0);
    m_suggestionsView->clear();
}

void SummaryPanel::setResult(const AnalysisResult& r) {
    populateOverview(r);
    populateTable(m_largestFilesTable, r.largestFiles, true);
    populateTable(m_oldFilesTable, r.oldFiles, true);

    // Largest dirs
    m_largestDirsTable->setRowCount(0);
    m_largestDirsTable->setSortingEnabled(false);
    for (const auto& e : r.largestDirs) {
        int row = m_largestDirsTable->rowCount();
        m_largestDirsTable->insertRow(row);
        m_largestDirsTable->setItem(row, 0, new QTableWidgetItem(e.path));
        auto* sizeItem = new QTableWidgetItem(FileNode::formatSize(e.size));
        sizeItem->setData(Qt::UserRole, e.size);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_largestDirsTable->setItem(row, 1, sizeItem);
        auto* countItem = new QTableWidgetItem(QString::number(e.fileCount));
        countItem->setData(Qt::UserRole, e.fileCount);
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_largestDirsTable->setItem(row, 2, countItem);
    }
    m_largestDirsTable->setSortingEnabled(true);

    populateTypeTable(m_byTypeTable, r.byType, r.totalSize);

    // Suggestions
    QString html;
    if (r.suggestions.isEmpty()) {
        html = "<p style='color:gray'>No specific suggestions at this time.</p>";
    } else {
        html = "<ul style='margin:12px'>";
        for (const QString& s : r.suggestions)
            html += QString("<li style='margin-bottom:8px'>%1</li>").arg(s.toHtmlEscaped());
        html += "</ul>";
    }
    if (!r.emptyDirs.isEmpty()) {
        html += QString("<p><b>Empty directories (%1):</b></p><ul>").arg(r.emptyDirs.size());
        int n = qMin(20, r.emptyDirs.size());
        for (int i = 0; i < n; ++i)
            html += QString("<li>%1</li>").arg(r.emptyDirs[i].toHtmlEscaped());
        if (r.emptyDirs.size() > 20)
            html += QString("<li><i>... and %1 more</i></li>").arg(r.emptyDirs.size() - 20);
        html += "</ul>";
    }
    m_suggestionsView->setHtml(html);
}

void SummaryPanel::populateOverview(const AnalysisResult& r) {
    QString html;
    html += "<style>td { padding: 4px 12px 4px 0; } "
            "b { color: #2060c0; }</style>";
    html += "<h2 style='margin-bottom:4px'>Scan Summary</h2>";
    html += "<table>";
    html += QString("<tr><td>Scanned paths:</td><td><b>%1</b></td></tr>")
                .arg(r.scannedPaths.join(", ").toHtmlEscaped());
    html += QString("<tr><td>Scan time:</td><td>%1</td></tr>")
                .arg(r.scanTime.toString("yyyy-MM-dd HH:mm:ss"));
    html += QString("<tr><td>Total size:</td><td><b>%1</b></td></tr>")
                .arg(FileNode::formatSize(r.totalSize));
    html += QString("<tr><td>Total files:</td><td><b>%1</b></td></tr>")
                .arg(QLocale().toString(r.totalFiles));
    html += QString("<tr><td>Total directories:</td><td><b>%1</b></td></tr>")
                .arg(QLocale().toString(r.totalDirs));

    if (r.oldFilesCount > 0) {
        html += QString("<tr><td>Files &gt; 1 yr old:</td><td><b>%1</b> (%2)</td></tr>")
                    .arg(QLocale().toString(r.oldFilesCount))
                    .arg(FileNode::formatSize(r.oldFilesSize));
    }
    if (!r.emptyDirs.isEmpty()) {
        html += QString("<tr><td>Empty directories:</td><td><b>%1</b></td></tr>")
                    .arg(r.emptyDirs.size());
    }
    html += "</table>";

    if (!r.largestFiles.isEmpty()) {
        html += "<h3 style='margin-top:16px'>Top 5 Largest Files</h3><ul>";
        for (int i = 0; i < qMin(5, r.largestFiles.size()); ++i) {
            const auto& e = r.largestFiles[i];
            html += QString("<li><b>%1</b> — %2</li>")
                        .arg(e.name.toHtmlEscaped())
                        .arg(FileNode::formatSize(e.size));
        }
        html += "</ul>";
    }

    if (!r.suggestions.isEmpty()) {
        html += "<h3 style='margin-top:16px;color:#c04000'>Cleanup Suggestions</h3><ul>";
        for (const QString& s : r.suggestions)
            html += QString("<li>%1</li>").arg(s.toHtmlEscaped());
        html += "</ul>";
    }

    m_overviewLabel->setText(html);
}

void SummaryPanel::populateTable(QTableWidget* table,
                                  const QList<AnalysisResult::Entry>& entries,
                                  bool showType) {
    table->setRowCount(0);
    table->setSortingEnabled(false);
    for (const auto& e : entries) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(e.path));

        auto* sizeItem = new QTableWidgetItem(FileNode::formatSize(e.size));
        sizeItem->setData(Qt::UserRole, e.size);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 1, sizeItem);

        table->setItem(row, 2, new QTableWidgetItem(
            e.lastAccessed.isValid() ? e.lastAccessed.toString("yyyy-MM-dd") : "Unknown"));

        table->setItem(row, 3, new QTableWidgetItem(
            e.lastModified.isValid() ? e.lastModified.toString("yyyy-MM-dd") : "Unknown"));

        if (showType && table->columnCount() > 4)
            table->setItem(row, 4, new QTableWidgetItem(e.extension.isEmpty() ? "-" : e.extension));
    }
    table->setSortingEnabled(true);
}

void SummaryPanel::populateTypeTable(QTableWidget* table,
                                      const QList<AnalysisResult::TypeEntry>& types,
                                      qint64 totalSize) {
    table->setRowCount(0);
    table->setSortingEnabled(false);
    for (const auto& t : types) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(
            t.extension.isEmpty() ? "(no extension)" : "." + t.extension));

        auto* sizeItem = new QTableWidgetItem(FileNode::formatSize(t.totalSize));
        sizeItem->setData(Qt::UserRole, t.totalSize);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 1, sizeItem);

        auto* countItem = new QTableWidgetItem(QString::number(t.count));
        countItem->setData(Qt::UserRole, t.count);
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 2, countItem);

        double pct = totalSize > 0 ? 100.0 * t.totalSize / totalSize : 0;
        auto* pctItem = new QTableWidgetItem(QString::number(pct, 'f', 1) + "%");
        pctItem->setData(Qt::UserRole, pct);
        pctItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 3, pctItem);
    }
    table->setSortingEnabled(true);
}
