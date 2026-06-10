#include <QtTest>
#include <QAbstractItemModelTester>
#include "filemodel.h"

namespace {

std::unique_ptr<FileNode> buildTree() {
    // root "/" → docs/ (two files), readme.txt
    auto root = std::make_unique<FileNode>();
    root->name = "/";
    root->path = "/";
    root->isDir = true;

    auto docs = std::make_unique<FileNode>();
    docs->parent = root.get();
    docs->rowInParent = 0;
    docs->name = "docs";
    docs->path = "/docs";
    docs->isDir = true;
    docs->size = 3000;
    docs->fileCount = 2;

    auto fileA = std::make_unique<FileNode>();
    fileA->parent = docs.get();
    fileA->rowInParent = 0;
    fileA->name = "a.pdf";
    fileA->path = "/docs/a.pdf";
    fileA->size = 2000;
    fileA->extension = "pdf";
    fileA->lastAccessed = QDateTime(QDate(2024, 3, 15), QTime(12, 0));
    fileA->lastModified = fileA->lastAccessed;
    docs->children.push_back(std::move(fileA));

    auto fileB = std::make_unique<FileNode>();
    fileB->parent = docs.get();
    fileB->rowInParent = 1;
    fileB->name = "b.txt";
    fileB->path = "/docs/b.txt";
    fileB->size = 1000;
    fileB->extension = "txt";
    docs->children.push_back(std::move(fileB));

    auto readme = std::make_unique<FileNode>();
    readme->parent = root.get();
    readme->rowInParent = 1;
    readme->name = "readme.txt";
    readme->path = "/readme.txt";
    readme->size = 500;
    readme->extension = "txt";

    root->size = 3500;
    root->fileCount = 3;
    root->children.push_back(std::move(docs));
    root->children.push_back(std::move(readme));
    return root;
}

}  // namespace

class TestFileModel : public QObject {
    Q_OBJECT
private slots:
    void modelIntegrity();
    void structure();
    void displayData();
    void customRoles();
    void headers();
    void clearResetsModel();
};

void TestFileModel::modelIntegrity() {
    // QAbstractItemModelTester exercises the full QAbstractItemModel contract
    // (index/parent consistency, signal correctness) on every model change.
    FileModel model;
    QAbstractItemModelTester tester(&model,
        QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRoot(buildTree());
    model.clear();
    model.setRoot(buildTree());
}

void TestFileModel::structure() {
    FileModel model;
    model.setRoot(buildTree());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), int(FileModel::ColCount));

    QModelIndex docs = model.index(0, 0);
    QVERIFY(docs.isValid());
    QCOMPARE(model.rowCount(docs), 2);
    QVERIFY(model.hasChildren(docs));

    QModelIndex fileA = model.index(0, 0, docs);
    QVERIFY(fileA.isValid());
    QCOMPARE(model.parent(fileA), docs);
    QVERIFY(!model.hasChildren(fileA));

    QModelIndex readme = model.index(1, 0);
    QCOMPARE(model.parent(readme), QModelIndex());
}

void TestFileModel::displayData() {
    FileModel model;
    model.setRoot(buildTree());

    QModelIndex docs = model.index(0, 0);
    QCOMPARE(docs.data(Qt::DisplayRole).toString(), QString("docs"));

    QModelIndex docsSize = model.index(0, FileModel::ColSize);
    QString sizeText = docsSize.data(Qt::DisplayRole).toString();
    QVERIFY(sizeText.contains("2.9 KB"));   // 3000 bytes
    QVERIFY(sizeText.contains("2 files"));

    QModelIndex fileA = model.index(0, 0, docs);
    QModelIndex fileADate = model.index(0, FileModel::ColLastAccessed, docs);
    QCOMPARE(fileA.data(Qt::DisplayRole).toString(), QString("a.pdf"));
    QCOMPARE(fileADate.data(Qt::DisplayRole).toString(), QString("2024-03-15"));

    // b.txt has no lastAccessed
    QModelIndex fileBDate = model.index(1, FileModel::ColLastAccessed, docs);
    QCOMPARE(fileBDate.data(Qt::DisplayRole).toString(), QString("Unknown"));
}

void TestFileModel::customRoles() {
    FileModel model;
    model.setRoot(buildTree());

    QModelIndex docs = model.index(0, 0);
    QCOMPARE(docs.data(FileModel::SizeRole).toLongLong(), qint64(3000));
    QCOMPARE(docs.data(FileModel::MaxSizeRole).toLongLong(), qint64(3500));
    QCOMPARE(docs.data(FileModel::IsDirectoryRole).toBool(), true);
    QCOMPARE(docs.data(FileModel::PathRole).toString(), QString("/docs"));

    QModelIndex fileA = model.index(0, 0, docs);
    QCOMPARE(fileA.data(FileModel::IsDirectoryRole).toBool(), false);
    QCOMPARE(fileA.data(FileModel::PathRole).toString(), QString("/docs/a.pdf"));
}

void TestFileModel::headers() {
    FileModel model;
    QCOMPARE(model.headerData(FileModel::ColName, Qt::Horizontal).toString(),
             QString("Name"));
    QCOMPARE(model.headerData(FileModel::ColSize, Qt::Horizontal).toString(),
             QString("Size"));
    QCOMPARE(model.headerData(FileModel::ColLastAccessed, Qt::Horizontal).toString(),
             QString("Last Accessed"));
    QCOMPARE(model.headerData(FileModel::ColModified, Qt::Horizontal).toString(),
             QString("Modified"));
}

void TestFileModel::clearResetsModel() {
    FileModel model;
    model.setRoot(buildTree());
    QCOMPARE(model.rowCount(), 2);
    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.root() == nullptr);
}

QTEST_GUILESS_MAIN(TestFileModel)
#include "tst_filemodel.moc"
