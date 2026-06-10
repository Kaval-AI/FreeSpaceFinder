#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "scanner.h"

namespace {

void createFile(const QString& dir, const QString& name, qint64 size) {
    QFile f(dir + "/" + name);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(f.errorString()));
    f.write(QByteArray(static_cast<int>(size), 'x'));
    f.close();
}

const FileNode* findChildNode(const FileNode* parent, const QString& name) {
    for (const auto& c : parent->children)
        if (c->name == name) return c.get();
    return nullptr;
}

}  // namespace

class TestScanner : public QObject {
    Q_OBJECT
private slots:
    void scanCollectsFilesAndSizes();
    void childrenSortedBySizeDescending();
    void nonexistentPathEmitsError();
    void cancelStopsScan();
};

void TestScanner::scanCollectsFilesAndSizes() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    createFile(tmp.path(), "a.txt", 100);
    QVERIFY(QDir(tmp.path()).mkpath("sub/nested"));
    createFile(tmp.path() + "/sub", "b.bin", 2500);
    createFile(tmp.path() + "/sub/nested", "c.txt", 50);
    QVERIFY(QDir(tmp.path()).mkpath("emptydir"));

    Scanner scanner;
    QSignalSpy finishedSpy(&scanner, &Scanner::finished);
    scanner.scan({tmp.path()});

    QCOMPARE(finishedSpy.count(), 1);
    std::unique_ptr<FileNode> root(finishedSpy.at(0).at(0).value<FileNode*>());
    QVERIFY(root != nullptr);
    QCOMPARE(finishedSpy.at(0).at(1).toBool(), false);  // not cancelled

    QCOMPARE(root->children.size(), size_t(1));
    const FileNode* top = root->children[0].get();
    QVERIFY(top->isDir);
    QCOMPARE(top->size, qint64(2650));
    QCOMPARE(top->fileCount, qint64(3));

    const FileNode* sub = findChildNode(top, "sub");
    QVERIFY(sub != nullptr);
    QCOMPARE(sub->size, qint64(2550));
    QCOMPARE(sub->fileCount, qint64(2));

    const FileNode* a = findChildNode(top, "a.txt");
    QVERIFY(a != nullptr);
    QVERIFY(!a->isDir);
    QCOMPARE(a->size, qint64(100));
    QCOMPARE(a->extension, QString("txt"));
    QVERIFY(a->lastModified.isValid());

    const FileNode* empty = findChildNode(top, "emptydir");
    QVERIFY(empty != nullptr);
    QVERIFY(empty->isDir);
    QVERIFY(empty->children.empty());
}

void TestScanner::childrenSortedBySizeDescending() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    createFile(tmp.path(), "small.dat", 10);
    createFile(tmp.path(), "large.dat", 5000);
    createFile(tmp.path(), "medium.dat", 300);

    Scanner scanner;
    QSignalSpy finishedSpy(&scanner, &Scanner::finished);
    scanner.scan({tmp.path()});

    QCOMPARE(finishedSpy.count(), 1);
    std::unique_ptr<FileNode> root(finishedSpy.at(0).at(0).value<FileNode*>());
    const FileNode* top = root->children[0].get();

    QCOMPARE(top->children.size(), size_t(3));
    QCOMPARE(top->children[0]->name, QString("large.dat"));
    QCOMPARE(top->children[1]->name, QString("medium.dat"));
    QCOMPARE(top->children[2]->name, QString("small.dat"));
    // rowInParent must match position after sorting
    for (int i = 0; i < 3; ++i)
        QCOMPARE(top->children[i]->rowInParent, i);
}

void TestScanner::nonexistentPathEmitsError() {
    Scanner scanner;
    QSignalSpy errorSpy(&scanner, &Scanner::errorOccurred);
    QSignalSpy finishedSpy(&scanner, &Scanner::finished);

    scanner.scan({"/nonexistent/path/xyz123"});

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains("does not exist"));
    // finished is still emitted (with an empty root)
    QCOMPARE(finishedSpy.count(), 1);
    std::unique_ptr<FileNode> root(finishedSpy.at(0).at(0).value<FileNode*>());
    QVERIFY(root->children.empty());
}

void TestScanner::cancelStopsScan() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Enough files to cross the progress-emission threshold (every 500 files)
    const int fileCount = 1200;
    for (int i = 0; i < fileCount; ++i)
        createFile(tmp.path(), QString("f%1.dat").arg(i), 1);

    Scanner scanner;
    QSignalSpy finishedSpy(&scanner, &Scanner::finished);
    // Cancel as soon as the first progress signal arrives
    connect(&scanner, &Scanner::progress, &scanner, [&scanner]() {
        scanner.cancel();
    });

    scanner.scan({tmp.path()});

    QCOMPARE(finishedSpy.count(), 1);
    std::unique_ptr<FileNode> root(finishedSpy.at(0).at(0).value<FileNode*>());
    QCOMPARE(finishedSpy.at(0).at(1).toBool(), true);  // cancelled
    QVERIFY2(root->fileCount < fileCount,
             qPrintable(QString("expected partial scan, got %1 files").arg(root->fileCount)));
}

QTEST_GUILESS_MAIN(TestScanner)
#include "tst_scanner.moc"
