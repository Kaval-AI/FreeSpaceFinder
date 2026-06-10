#pragma once
#include "analyzer.h"
#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTextBrowser>

class SummaryPanel : public QWidget {
    Q_OBJECT
public:
    explicit SummaryPanel(QWidget* parent = nullptr);

    void setResult(const AnalysisResult& result);
    void clear();

private:
    void populateOverview(const AnalysisResult& r);
    void populateTable(QTableWidget* table, const QList<AnalysisResult::Entry>& entries, bool showType);
    void populateTypeTable(QTableWidget* table, const QList<AnalysisResult::TypeEntry>& types,
                           qint64 totalSize);

    QTabWidget* m_tabs;
    QLabel*      m_overviewLabel;
    QTableWidget* m_largestFilesTable;
    QTableWidget* m_largestDirsTable;
    QTableWidget* m_byTypeTable;
    QTableWidget* m_oldFilesTable;
    QTextBrowser* m_suggestionsView;
};
