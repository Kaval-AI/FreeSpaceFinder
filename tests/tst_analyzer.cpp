#include <QtTest>
#include <QDir>
#include "analyzer.h"
#include "filenode.h"

namespace {

FileNode* addDir(FileNode* parent, const QString& name) {
    auto node = std::make_unique<FileNode>();
    node->parent = parent;
    node->rowInParent = static_cast<int>(parent->children.size());
    node->name = name;
    node->path = (parent->path == "/" ? "" : parent->path) + "/" + name;
    node->isDir = true;
    FileNode* raw = node.get();
    parent->children.push_back(std::move(node));
    return raw;
}

FileNode* addFile(FileNode* parent, const QString& name, qint64 size, int ageDays) {
    auto node = std::make_unique<FileNode>();
    node->parent = parent;
    node->rowInParent = static_cast<int>(parent->children.size());
    node->name = name;
    node->path = (parent->path == "/" ? "" : parent->path) + "/" + name;
    node->size = size;
    int dot = name.lastIndexOf('.');
    node->extension = dot >= 0 ? name.mid(dot + 1).toLower() : QString();
    node->lastAccessed = QDateTime::currentDateTime().addDays(-ageDays);
    node->lastModified = node->lastAccessed;
    FileNode* raw = node.get();
    parent->children.push_back(std::move(node));
    for (FileNode* p = parent; p; p = p->parent) {
        p->size += size;
        ++p->fileCount;
    }
    return raw;
}

constexpr qint64 MB = 1024LL * 1024;

}  // namespace

class TestAnalyzer : public QObject {
    Q_OBJECT
private slots:
    void init();
    void totals();
    void largestFilesSorted();
    void oldFileDetection();
    void typeBreakdown();
    void emptyDirsAndSuggestions();
    void aiContextContent();
    void diskInfoForRealPath();
    void nullRootIsInvalid();

private:
    std::unique_ptr<FileNode> m_root;
    AnalysisResult m_result;
};

void TestAnalyzer::init() {
    // root "/"
    //  └── data/
    //       ├── big.iso      300 MB, accessed 10 days ago
    //       ├── old.zip      150 MB, accessed 2000 days ago
    //       ├── notes.txt      1 KB, accessed 5 days ago
    //       ├── logs/
    //       │    └── app.log   1 KB, accessed 5 days ago
    //       └── empty/
    m_root = std::make_unique<FileNode>();
    m_root->name = "/";
    m_root->path = "/";
    m_root->isDir = true;

    FileNode* data = addDir(m_root.get(), "data");
    addFile(data, "big.iso", 300 * MB, 10);
    addFile(data, "old.zip", 150 * MB, 2000);
    addFile(data, "notes.txt", 1024, 5);
    FileNode* logs = addDir(data, "logs");
    addFile(logs, "app.log", 1024, 5);
    addDir(data, "empty");

    Analyzer analyzer;
    m_result = analyzer.analyze(m_root.get(), {"/data"});
}

void TestAnalyzer::totals() {
    QVERIFY(m_result.isValid);
    QCOMPARE(m_result.totalFiles, qint64(4));
    QCOMPARE(m_result.totalDirs, qint64(3));  // data, logs, empty
    QCOMPARE(m_result.totalSize, 450 * MB + 2048);
}

void TestAnalyzer::largestFilesSorted() {
    QVERIFY(m_result.largestFiles.size() >= 2);
    QCOMPARE(m_result.largestFiles[0].name, QString("big.iso"));
    QCOMPARE(m_result.largestFiles[0].size, 300 * MB);
    QCOMPARE(m_result.largestFiles[1].name, QString("old.zip"));
    QVERIFY(!m_result.largestDirs.isEmpty());
    QCOMPARE(m_result.largestDirs[0].path, QString("/data"));
}

void TestAnalyzer::oldFileDetection() {
    QCOMPARE(m_result.oldFilesCount, qint64(1));
    QCOMPARE(m_result.oldFilesSize, 150 * MB);
    QCOMPARE(m_result.oldFiles[0].name, QString("old.zip"));
    // 2000 days > 3 years → also "very old"
    QCOMPARE(m_result.veryOldFilesCount, qint64(1));
    QCOMPARE(m_result.veryOldFiles[0].name, QString("old.zip"));
}

void TestAnalyzer::typeBreakdown() {
    QVERIFY(!m_result.byType.isEmpty());
    // Sorted by total size descending → .iso first
    QCOMPARE(m_result.byType[0].extension, QString("iso"));
    QCOMPARE(m_result.byType[0].totalSize, 300 * MB);
    QCOMPARE(m_result.byType[0].count, 1);

    bool foundTxt = false;
    for (const auto& t : m_result.byType) {
        if (t.extension == "txt") {
            foundTxt = true;
            QCOMPARE(t.totalSize, qint64(1024));
        }
    }
    QVERIFY(foundTxt);
}

void TestAnalyzer::emptyDirsAndSuggestions() {
    QCOMPARE(m_result.emptyDirs.size(), 1);
    QCOMPARE(m_result.emptyDirs[0], QString("/data/empty"));

    // 150 MB of >1yr-old files (>100 MB threshold) → cleanup suggestion
    bool hasOldSuggestion = false;
    bool hasEmptyDirSuggestion = false;
    bool hasLargestFileSuggestion = false;
    for (const QString& s : m_result.suggestions) {
        if (s.contains("not accessed in over a year")) hasOldSuggestion = true;
        if (s.contains("empty directories")) hasEmptyDirSuggestion = true;
        if (s.contains("big.iso")) hasLargestFileSuggestion = true;
    }
    QVERIFY(hasOldSuggestion);
    QVERIFY(hasEmptyDirSuggestion);
    QVERIFY(hasLargestFileSuggestion);
}

void TestAnalyzer::aiContextContent() {
    QVERIFY(m_result.aiContext.contains("DISK SPACE ANALYSIS REPORT"));
    QVERIFY(m_result.aiContext.contains("/data/big.iso"));
    QVERIFY(m_result.aiContext.contains("/data/old.zip"));
    QVERIFY(m_result.aiContext.contains("Total files: 4"));
}

void TestAnalyzer::diskInfoForRealPath() {
    // "/data" from init() doesn't exist, so m_result has no disk entries.
    // Analyze with a real path to exercise the QStorageInfo lookup.
    Analyzer analyzer;
    AnalysisResult r = analyzer.analyze(m_root.get(),
                                        {QDir::tempPath(), QDir::tempPath()});
    QCOMPARE(r.disks.size(), 1);  // duplicates collapse to one volume
    QVERIFY(r.disks[0].totalBytes > 0);
    QVERIFY(r.disks[0].freeBytes >= 0);
    QVERIFY(r.disks[0].freeBytes <= r.disks[0].totalBytes);
    QVERIFY(!r.disks[0].mountPoint.isEmpty());
    QVERIFY(r.aiContext.contains("--- DISKS"));
    QVERIFY(r.aiContext.contains(r.disks[0].mountPoint));
}

void TestAnalyzer::nullRootIsInvalid() {
    Analyzer analyzer;
    AnalysisResult r = analyzer.analyze(nullptr, {});
    QVERIFY(!r.isValid);
    QCOMPARE(r.totalSize, qint64(0));
}

QTEST_GUILESS_MAIN(TestAnalyzer)
#include "tst_analyzer.moc"
