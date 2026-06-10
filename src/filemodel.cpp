#include "filemodel.h"

FileModel::FileModel(QObject* parent) : QAbstractItemModel(parent) {}

void FileModel::setRoot(std::unique_ptr<FileNode> root) {
    beginResetModel();
    m_root = std::move(root);
    m_totalSize = m_root ? m_root->size : 0;
    endResetModel();
}

void FileModel::clear() {
    beginResetModel();
    m_root.reset();
    m_totalSize = 0;
    endResetModel();
}

FileNode* FileModel::nodeFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) return m_root.get();
    return static_cast<FileNode*>(index.internalPointer());
}

QModelIndex FileModel::index(int row, int column, const QModelIndex& parentIdx) const {
    if (!m_root) return {};
    FileNode* parentNode = nodeFromIndex(parentIdx);
    if (!parentNode) return {};
    if (row < 0 || row >= static_cast<int>(parentNode->children.size())) return {};
    return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex FileModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return {};
    auto* node = static_cast<FileNode*>(child.internalPointer());
    if (!node) return {};
    FileNode* parentNode = node->parent;
    if (!parentNode || parentNode == m_root.get()) return {};
    return createIndex(parentNode->rowInParent, 0, parentNode);
}

int FileModel::rowCount(const QModelIndex& parentIdx) const {
    if (!m_root) return 0;
    FileNode* node = nodeFromIndex(parentIdx);
    if (!node) return 0;
    return static_cast<int>(node->children.size());
}

int FileModel::columnCount(const QModelIndex&) const {
    return ColCount;
}

bool FileModel::hasChildren(const QModelIndex& parentIdx) const {
    if (!m_root) return false;
    FileNode* node = nodeFromIndex(parentIdx);
    if (!node) return false;
    return !node->children.empty();
}

QVariant FileModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    auto* node = static_cast<FileNode*>(index.internalPointer());
    if (!node) return {};

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (index.column()) {
        case ColName:
            return node->name;
        case ColSize:
            if (node->isDir)
                return QString("%1  (%2 files)").arg(FileNode::formatSize(node->size)).arg(node->fileCount);
            return FileNode::formatSize(node->size);
        case ColLastAccessed:
            return node->lastAccessed.isValid()
                ? node->lastAccessed.toString("yyyy-MM-dd")
                : QString("Unknown");
        case ColModified:
            return node->lastModified.isValid()
                ? node->lastModified.toString("yyyy-MM-dd")
                : QString("Unknown");
        }
    }

    if (role == Qt::DecorationRole && index.column() == ColName) {
        return node->isDir ? QStringLiteral("📁") : QStringLiteral("📄");
    }

    if (role == SizeRole) return node->size;
    if (role == MaxSizeRole) return m_totalSize;
    if (role == IsDirectoryRole) return node->isDir;

    if (role == Qt::TextAlignmentRole && index.column() == ColSize)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    return {};
}

QVariant FileModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColName: return "Name";
    case ColSize: return "Size";
    case ColLastAccessed: return "Last Accessed";
    case ColModified: return "Modified";
    }
    return {};
}
