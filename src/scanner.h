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

public:
    // Thread-safe: only sets an atomic flag. Call directly from any thread —
    // do NOT invoke via a queued connection, the worker's event loop is busy
    // inside scan() and would only deliver it after the scan completes.
    void cancel() { m_cancelled.store(true); }

signals:
    void progress(const QString& currentPath, qint64 filesScanned, qint64 totalSize);
    void finished(FileNode* root, bool cancelled);   // receiver takes ownership of root
    void errorOccurred(const QString& message);

private:
    void scanDirectory(FileNode* parentNode, const QString& path);

    std::atomic<bool> m_cancelled{false};
    qint64 m_filesScanned = 0;
    qint64 m_totalSize = 0;
};
