#pragma once
#include "filenode.h"
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QList>

struct AnalysisResult {
    struct Entry {
        QString path;
        QString name;
        qint64 size = 0;
        QDateTime lastAccessed;
        QDateTime lastModified;
        QString extension;
        bool isDir = false;
        qint64 fileCount = 0;
    };

    struct TypeEntry {
        QString extension;
        qint64 totalSize = 0;
        int count = 0;
    };

    struct DiskEntry {
        QString mountPoint;   // volume root, e.g. "/" or "C:/"
        QString device;       // e.g. "/dev/nvme0n1p2"
        qint64 totalBytes = 0;
        qint64 freeBytes = 0;
    };

    // Overview
    QStringList scannedPaths;
    QDateTime scanTime;
    qint64 totalSize = 0;
    qint64 totalFiles = 0;
    qint64 totalDirs = 0;

    // Lists
    QList<Entry> largestFiles;
    QList<Entry> largestDirs;
    QList<TypeEntry> byType;
    QList<Entry> oldFiles;          // not accessed in > 365 days
    QList<Entry> veryOldFiles;      // not accessed in > 3 years
    QStringList emptyDirs;
    QList<DiskEntry> disks;         // volumes containing the scanned paths

    // Aggregates
    qint64 oldFilesSize = 0;
    qint64 oldFilesCount = 0;
    qint64 veryOldFilesSize = 0;
    qint64 veryOldFilesCount = 0;

    // Suggestions
    QStringList suggestions;

    QString aiContext;  // pre-built context string for AI
    bool isValid = false;
};

class Analyzer {
public:
    AnalysisResult analyze(const FileNode* root, const QStringList& paths);

private:
    QString buildAIContext(const AnalysisResult& r) const;
};
