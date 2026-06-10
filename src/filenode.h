#pragma once
#include <QString>
#include <QDateTime>
#include <vector>
#include <memory>

struct FileNode {
    QString name;
    QString path;
    qint64 size = 0;
    QDateTime lastModified;
    QDateTime lastAccessed;
    bool isDir = false;
    bool isSymLink = false;
    QString extension;      // lowercase, no dot; empty for dirs
    qint64 fileCount = 0;   // recursive file count (dirs only)
    int rowInParent = 0;    // cached index for O(1) parent() in model
    FileNode* parent = nullptr;
    std::vector<std::unique_ptr<FileNode>> children;

    static QString formatSize(qint64 bytes) {
        if (bytes < 0) return "?";
        constexpr qint64 KB = 1024, MB = KB * 1024, GB = MB * 1024;
        if (bytes < KB) return QString::number(bytes) + " B";
        if (bytes < MB) return QString::number(bytes / (double)KB, 'f', 1) + " KB";
        if (bytes < GB) return QString::number(bytes / (double)MB, 'f', 1) + " MB";
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    }

    static QString formatAge(const QDateTime& dt) {
        if (!dt.isValid()) return "Unknown";
        qint64 days = dt.daysTo(QDateTime::currentDateTime());
        if (days < 0) return "Future";
        if (days == 0) return "Today";
        if (days == 1) return "Yesterday";
        if (days < 7) return QString("%1 days ago").arg(days);
        if (days < 31) return QString("%1 weeks ago").arg(days / 7);
        if (days < 365) return QString("%1 months ago").arg(days / 30);
        return QString("%1 yr%2 ago").arg(days / 365.0, 0, 'f', 1).arg(days / 365.0 >= 2.0 ? "s" : "");
    }

    qint64 ageInDays() const {
        if (!lastAccessed.isValid()) return 99999;
        return lastAccessed.daysTo(QDateTime::currentDateTime());
    }
};
