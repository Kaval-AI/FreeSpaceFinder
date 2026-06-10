#pragma once
#include "filenode.h"
#include <QObject>
#include <QStringList>
#include <atomic>

class Scanner : public QObject {
    Q_OBJECT
public:
    explicit Scanner(QObject* parent = nullptr);

public slots:
    void scan(const QStringList& paths);
    void cancel() { m_cancelled.store(true); }

signals:
    void progress(const QString& currentPath, qint64 filesScanned, qint64 totalSize);
    void finished(FileNode* root);   // receiver takes ownership
    void errorOccurred(const QString& message);

private:
    void scanDirectory(FileNode* parentNode, const QString& path);

    std::atomic<bool> m_cancelled{false};
    qint64 m_filesScanned = 0;
    qint64 m_totalSize = 0;
};
