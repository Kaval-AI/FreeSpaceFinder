#pragma once
#include "filenode.h"
#include <QAbstractItemModel>
#include <memory>

class FileModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { ColName = 0, ColSize, ColLastAccessed, ColModified, ColCount };
    enum UserRole { SizeRole = Qt::UserRole + 1, MaxSizeRole, IsDirectoryRole };

    explicit FileModel(QObject* parent = nullptr);

    void setRoot(std::unique_ptr<FileNode> root);
    void clear();
    const FileNode* root() const { return m_root.get(); }

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;

private:
    FileNode* nodeFromIndex(const QModelIndex& index) const;

    std::unique_ptr<FileNode> m_root;
    qint64 m_totalSize = 0;
};
