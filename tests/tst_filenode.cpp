#include <QtTest>
#include "filenode.h"

class TestFileNode : public QObject {
    Q_OBJECT
private slots:
    void formatSize_data();
    void formatSize();
    void formatAge();
    void ageInDays();
};

void TestFileNode::formatSize_data() {
    QTest::addColumn<qint64>("bytes");
    QTest::addColumn<QString>("expected");

    QTest::newRow("zero")      << qint64(0)                       << "0 B";
    QTest::newRow("bytes")     << qint64(512)                     << "512 B";
    QTest::newRow("just-1KB")  << qint64(1024)                    << "1.0 KB";
    QTest::newRow("1.5KB")     << qint64(1536)                    << "1.5 KB";
    QTest::newRow("5MB")       << qint64(5) * 1024 * 1024         << "5.0 MB";
    QTest::newRow("3GB")       << qint64(3) * 1024 * 1024 * 1024  << "3.00 GB";
    QTest::newRow("negative")  << qint64(-1)                      << "?";
}

void TestFileNode::formatSize() {
    QFETCH(qint64, bytes);
    QFETCH(QString, expected);
    QCOMPARE(FileNode::formatSize(bytes), expected);
}

void TestFileNode::formatAge() {
    QCOMPARE(FileNode::formatAge(QDateTime()), QString("Unknown"));
    QCOMPARE(FileNode::formatAge(QDateTime::currentDateTime()), QString("Today"));
    QCOMPARE(FileNode::formatAge(QDateTime::currentDateTime().addDays(-1)),
             QString("Yesterday"));
    QCOMPARE(FileNode::formatAge(QDateTime::currentDateTime().addDays(-3)),
             QString("3 days ago"));
    QCOMPARE(FileNode::formatAge(QDateTime::currentDateTime().addDays(-14)),
             QString("2 weeks ago"));
    QCOMPARE(FileNode::formatAge(QDateTime::currentDateTime().addDays(-90)),
             QString("3 months ago"));
    QVERIFY(FileNode::formatAge(QDateTime::currentDateTime().addDays(-800))
                .contains("yr"));
}

void TestFileNode::ageInDays() {
    FileNode node;
    QCOMPARE(node.ageInDays(), qint64(99999));  // invalid lastAccessed

    node.lastAccessed = QDateTime::currentDateTime().addDays(-10);
    QCOMPARE(node.ageInDays(), qint64(10));

    node.lastAccessed = QDateTime::currentDateTime();
    QCOMPARE(node.ageInDays(), qint64(0));
}

QTEST_GUILESS_MAIN(TestFileNode)
#include "tst_filenode.moc"
