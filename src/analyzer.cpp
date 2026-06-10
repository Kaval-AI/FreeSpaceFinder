#include "analyzer.h"
#include <QMap>
#include <QSet>
#include <QStorageInfo>
#include <algorithm>

AnalysisResult Analyzer::analyze(const FileNode* root, const QStringList& paths) {
    AnalysisResult r;
    r.scannedPaths = paths;
    r.scanTime = QDateTime::currentDateTime();
    r.totalSize = root ? root->size : 0;

    // Volumes the scanned paths live on (deduplicated by mount point)
    QSet<QString> seenMounts;
    for (const QString& p : paths) {
        QStorageInfo storage(p);
        if (!storage.isValid() || !storage.isReady())
            continue;
        if (seenMounts.contains(storage.rootPath()))
            continue;
        seenMounts.insert(storage.rootPath());
        AnalysisResult::DiskEntry d;
        d.mountPoint = storage.rootPath();
        d.device = QString::fromUtf8(storage.device());
        d.totalBytes = storage.bytesTotal();
        d.freeBytes = storage.bytesAvailable();
        r.disks.append(d);
    }

    if (!root) {
        r.aiContext = "No scan data available.";
        return r;
    }

    // Iterative BFS / DFS — avoids deep recursion on large trees
    QList<const FileNode*> stack;
    for (const auto& child : root->children)
        stack.append(child.get());

    QMap<QString, AnalysisResult::TypeEntry> typeMap;

    while (!stack.isEmpty()) {
        const FileNode* node = stack.takeLast();

        if (node->isDir) {
            ++r.totalDirs;
            AnalysisResult::Entry e;
            e.path = node->path;
            e.name = node->name;
            e.size = node->size;
            e.lastAccessed = node->lastAccessed;
            e.lastModified = node->lastModified;
            e.isDir = true;
            e.fileCount = node->fileCount;
            r.largestDirs.append(e);

            if (node->children.empty())
                r.emptyDirs.append(node->path);

            for (const auto& c : node->children)
                stack.append(c.get());
        } else {
            ++r.totalFiles;
            AnalysisResult::Entry e;
            e.path = node->path;
            e.name = node->name;
            e.size = node->size;
            e.lastAccessed = node->lastAccessed;
            e.lastModified = node->lastModified;
            e.extension = node->extension;
            e.isDir = false;
            r.largestFiles.append(e);

            auto& te = typeMap[node->extension];
            te.extension = node->extension;
            te.totalSize += node->size;
            ++te.count;

            qint64 days = node->ageInDays();
            if (days > 365) {
                r.oldFiles.append(e);
                r.oldFilesSize += node->size;
                ++r.oldFilesCount;
            }
            if (days > 365 * 3) {
                r.veryOldFiles.append(e);
                r.veryOldFilesSize += node->size;
                ++r.veryOldFilesCount;
            }
        }
    }

    for (const auto& te : typeMap)
        r.byType.append(te);

    auto bySizeDesc = [](const AnalysisResult::Entry& a, const AnalysisResult::Entry& b) {
        return a.size > b.size;
    };
    std::sort(r.largestFiles.begin(), r.largestFiles.end(), bySizeDesc);
    std::sort(r.largestDirs.begin(), r.largestDirs.end(), bySizeDesc);
    std::sort(r.oldFiles.begin(), r.oldFiles.end(), bySizeDesc);
    std::sort(r.veryOldFiles.begin(), r.veryOldFiles.end(), bySizeDesc);
    std::sort(r.byType.begin(), r.byType.end(),
              [](const AnalysisResult::TypeEntry& a, const AnalysisResult::TypeEntry& b) {
                  return a.totalSize > b.totalSize;
              });

    if (r.largestFiles.size() > 50) r.largestFiles.resize(50);
    if (r.largestDirs.size() > 30) r.largestDirs.resize(30);
    if (r.oldFiles.size() > 50) r.oldFiles.resize(50);
    if (r.veryOldFiles.size() > 20) r.veryOldFiles.resize(20);

    // Suggestions
    if (r.oldFilesSize > 100LL * 1024 * 1024) {
        r.suggestions << QString("Free up to %1 by removing %2 files not accessed in over a year.")
                            .arg(FileNode::formatSize(r.oldFilesSize))
                            .arg(r.oldFilesCount);
    }
    if (!r.emptyDirs.isEmpty()) {
        r.suggestions << QString("Remove %1 empty directories.").arg(r.emptyDirs.size());
    }
    for (const auto& t : r.byType) {
        if (t.extension.isEmpty()) continue;
        constexpr qint64 HALF_GB = 512LL * 1024 * 1024;
        if (t.totalSize > HALF_GB &&
            (t.extension == "tmp" || t.extension == "log" || t.extension == "bak")) {
            r.suggestions << QString("Found %1 of .%2 files — consider cleaning them up.")
                                .arg(FileNode::formatSize(t.totalSize))
                                .arg(t.extension);
        }
    }
    if (!r.largestFiles.isEmpty() && r.largestFiles.first().size > 200LL * 1024 * 1024) {
        r.suggestions << QString("Largest file: %1 (%2) — review if still needed.")
                            .arg(r.largestFiles.first().name)
                            .arg(FileNode::formatSize(r.largestFiles.first().size));
    }

    r.aiContext = buildAIContext(r);
    r.isValid = true;
    return r;
}

QString Analyzer::buildAIContext(const AnalysisResult& r) const {
    QString ctx;
    ctx.reserve(32768);

    ctx += "=== DISK SPACE ANALYSIS REPORT ===\n\n";
    ctx += QString("Scan performed: %1\n").arg(r.scanTime.toString("yyyy-MM-dd HH:mm"));
    ctx += QString("Scanned paths: %1\n").arg(r.scannedPaths.join(", "));
    ctx += QString("Total size: %1\n").arg(FileNode::formatSize(r.totalSize));
    ctx += QString("Total files: %1\n").arg(r.totalFiles);
    ctx += QString("Total directories: %1\n\n").arg(r.totalDirs);

    if (!r.disks.isEmpty()) {
        ctx += "--- DISKS (volumes containing the scanned paths) ---\n";
        for (const auto& d : r.disks) {
            qint64 used = d.totalBytes - d.freeBytes;
            double usedPct = d.totalBytes > 0 ? 100.0 * used / d.totalBytes : 0.0;
            ctx += QString("  %1 (%2): total %3 | used %4 (%5%) | free %6\n")
                       .arg(d.mountPoint)
                       .arg(d.device.isEmpty() ? "unknown device" : d.device)
                       .arg(FileNode::formatSize(d.totalBytes))
                       .arg(FileNode::formatSize(used))
                       .arg(usedPct, 0, 'f', 1)
                       .arg(FileNode::formatSize(d.freeBytes));
        }
        ctx += "\n";
    }

    ctx += "--- TOP 20 LARGEST FILES ---\n";
    int nf = qMin(20, r.largestFiles.size());
    for (int i = 0; i < nf; ++i) {
        const auto& e = r.largestFiles[i];
        ctx += QString("  %1. %2\n     Size: %3")
                   .arg(i + 1).arg(e.path).arg(FileNode::formatSize(e.size));
        if (e.lastAccessed.isValid())
            ctx += QString(" | Last accessed: %1 (%2)")
                       .arg(e.lastAccessed.toString("yyyy-MM-dd"))
                       .arg(FileNode::formatAge(e.lastAccessed));
        ctx += "\n";
    }

    ctx += "\n--- TOP 15 LARGEST DIRECTORIES ---\n";
    int nd = qMin(15, r.largestDirs.size());
    for (int i = 0; i < nd; ++i) {
        const auto& e = r.largestDirs[i];
        ctx += QString("  %1. %2\n     Size: %3 | Files: %4\n")
                   .arg(i + 1).arg(e.path)
                   .arg(FileNode::formatSize(e.size)).arg(e.fileCount);
    }

    if (!r.byType.isEmpty()) {
        ctx += "\n--- FILE TYPE BREAKDOWN (by size, top 20) ---\n";
        int nt = qMin(20, r.byType.size());
        for (int i = 0; i < nt; ++i) {
            const auto& t = r.byType[i];
            double pct = r.totalSize > 0 ? 100.0 * t.totalSize / r.totalSize : 0;
            ctx += QString("  .%1: %2 across %3 files (%4%)\n")
                       .arg(t.extension.isEmpty() ? "(no ext)" : t.extension)
                       .arg(FileNode::formatSize(t.totalSize))
                       .arg(t.count)
                       .arg(pct, 0, 'f', 1);
        }
    }

    ctx += "\n--- FILES NOT ACCESSED IN OVER 1 YEAR ---\n";
    ctx += QString("Count: %1 | Total size: %2\n")
               .arg(r.oldFilesCount).arg(FileNode::formatSize(r.oldFilesSize));
    int no = qMin(10, r.oldFiles.size());
    for (int i = 0; i < no; ++i) {
        const auto& e = r.oldFiles[i];
        ctx += QString("  - %1 (%2, last: %3)\n")
                   .arg(e.path).arg(FileNode::formatSize(e.size))
                   .arg(e.lastAccessed.isValid()
                            ? e.lastAccessed.toString("yyyy-MM-dd") : "unknown");
    }

    if (r.veryOldFilesCount > 0) {
        ctx += QString("\n--- FILES NOT ACCESSED IN OVER 3 YEARS ---\n");
        ctx += QString("Count: %1 | Total size: %2\n")
                   .arg(r.veryOldFilesCount).arg(FileNode::formatSize(r.veryOldFilesSize));
    }

    if (!r.emptyDirs.isEmpty()) {
        ctx += QString("\n--- EMPTY DIRECTORIES: %1 ---\n").arg(r.emptyDirs.size());
        int ne = qMin(10, r.emptyDirs.size());
        for (int i = 0; i < ne; ++i)
            ctx += QString("  - %1\n").arg(r.emptyDirs[i]);
    }

    if (!r.suggestions.isEmpty()) {
        ctx += "\n--- CLEANUP SUGGESTIONS ---\n";
        for (int i = 0; i < r.suggestions.size(); ++i)
            ctx += QString("  %1. %2\n").arg(i + 1).arg(r.suggestions[i]);
    }

    ctx += "\n=== END OF REPORT ===\n\n";
    ctx += "You are a helpful disk space analysis assistant. Use the report above to answer "
           "questions about disk usage. Only reference data from this report — do not access "
           "the filesystem directly. Be specific with file paths, sizes, and access dates. "
           "Suggest practical cleanup actions where relevant.";

    return ctx;
}
