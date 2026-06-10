#include "scanner.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

Scanner::Scanner(QObject* parent) : QObject(parent) {}

void Scanner::scan(const QStringList& paths) {
    m_cancelled.store(false);
    m_filesScanned = 0;
    m_totalSize = 0;

    auto* root = new FileNode;
    root->name = "/";
    root->path = "/";
    root->isDir = true;

    for (const QString& p : paths) {
        if (m_cancelled.load()) break;
        QFileInfo fi(p);
        if (!fi.exists()) {
            emit errorOccurred(QString("Path does not exist: %1").arg(p));
            continue;
        }
        if (fi.isDir()) {
            auto node = std::make_unique<FileNode>();
            node->parent = root;
            node->rowInParent = static_cast<int>(root->children.size());
            node->name = fi.fileName().isEmpty() ? p : fi.fileName();
            node->path = fi.absoluteFilePath();
            node->isDir = true;
            node->lastModified = fi.lastModified();
            node->lastAccessed = fi.lastRead();
            scanDirectory(node.get(), fi.absoluteFilePath());
            root->size += node->size;
            root->fileCount += node->fileCount;
            root->children.push_back(std::move(node));
        } else {
            auto node = std::make_unique<FileNode>();
            node->parent = root;
            node->rowInParent = static_cast<int>(root->children.size());
            node->name = fi.fileName();
            node->path = fi.absoluteFilePath();
            node->isDir = false;
            node->isSymLink = fi.isSymLink();
            node->size = fi.size();
            node->lastModified = fi.lastModified();
            node->lastAccessed = fi.lastRead();
            QString ext = fi.suffix().toLower();
            node->extension = ext;
            root->size += node->size;
            ++root->fileCount;
            ++m_filesScanned;
            m_totalSize += node->size;
            root->children.push_back(std::move(node));
        }
    }

    // Sort each level by size descending
    std::sort(root->children.begin(), root->children.end(),
              [](const auto& a, const auto& b) { return a->size > b->size; });
    for (int i = 0; i < static_cast<int>(root->children.size()); ++i)
        root->children[i]->rowInParent = i;

    emit finished(root);
}

void Scanner::scanDirectory(FileNode* dirNode, const QString& path) {
    QDir dir(path);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    dir.setSorting(QDir::NoSort);

    const QFileInfoList entries = dir.entryInfoList();

    for (const QFileInfo& fi : entries) {
        if (m_cancelled.load()) return;

        if (fi.isSymLink()) {
            // Include symlinks as files but don't follow them
            auto node = std::make_unique<FileNode>();
            node->parent = dirNode;
            node->rowInParent = static_cast<int>(dirNode->children.size());
            node->name = fi.fileName();
            node->path = fi.absoluteFilePath();
            node->isDir = false;
            node->isSymLink = true;
            node->size = fi.size();
            node->lastModified = fi.lastModified();
            node->lastAccessed = fi.lastRead();
            node->extension = fi.suffix().toLower();
            dirNode->size += node->size;
            ++dirNode->fileCount;
            ++m_filesScanned;
            m_totalSize += node->size;
            dirNode->children.push_back(std::move(node));
        } else if (fi.isDir()) {
            auto node = std::make_unique<FileNode>();
            node->parent = dirNode;
            node->rowInParent = static_cast<int>(dirNode->children.size());
            node->name = fi.fileName();
            node->path = fi.absoluteFilePath();
            node->isDir = true;
            node->lastModified = fi.lastModified();
            node->lastAccessed = fi.lastRead();
            scanDirectory(node.get(), fi.absoluteFilePath());
            dirNode->size += node->size;
            dirNode->fileCount += node->fileCount;
            dirNode->children.push_back(std::move(node));
        } else if (fi.isFile()) {
            auto node = std::make_unique<FileNode>();
            node->parent = dirNode;
            node->rowInParent = static_cast<int>(dirNode->children.size());
            node->name = fi.fileName();
            node->path = fi.absoluteFilePath();
            node->isDir = false;
            node->size = fi.size();
            node->lastModified = fi.lastModified();
            node->lastAccessed = fi.lastRead();
            node->extension = fi.suffix().toLower();
            dirNode->size += node->size;
            ++dirNode->fileCount;
            ++m_filesScanned;
            m_totalSize += node->size;
            dirNode->children.push_back(std::move(node));
        }

        if (m_filesScanned % 500 == 0) {
            emit progress(fi.absoluteFilePath(), m_filesScanned, m_totalSize);
        }
    }

    // Sort children by size descending
    std::sort(dirNode->children.begin(), dirNode->children.end(),
              [](const auto& a, const auto& b) { return a->size > b->size; });
    for (int i = 0; i < static_cast<int>(dirNode->children.size()); ++i)
        dirNode->children[i]->rowInParent = i;
}
