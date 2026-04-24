#pragma once

#include <QAbstractItemModel>
#include <QList>
#include "databasemanager.h"

// ---------------------------------------------------------------------------
// TreeItem  —  in-memory mirror of a single DB node
// ---------------------------------------------------------------------------

class TreeItem
{
public:
    explicit TreeItem(const NodeData& data, TreeItem* parent = nullptr);
    ~TreeItem();

    void appendChild(TreeItem* child);
    void removeChild(int row);
    void insertChild(int row, TreeItem* child);

    TreeItem* child(int row);
    TreeItem* parentItem();
    int       childCount() const;
    int       row() const;

    NodeData& data();
    const NodeData& data() const;

private:
    NodeData          m_data;
    QList<TreeItem*>  m_children;
    TreeItem*         m_parent;
};

// ---------------------------------------------------------------------------
// TreeModel  —  QAbstractItemModel wrapping the node tree
// ---------------------------------------------------------------------------

class TreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit TreeModel(int userId, bool isAdmin, QObject* parent = nullptr);
    ~TreeModel() override;

    // QAbstractItemModel interface
    QModelIndex   index(int row, int column,
                        const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool          setData(const QModelIndex& index, const QVariant& value,
                          int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;

    // Drag & drop (for moving nodes)
    Qt::DropActions supportedDropActions() const override;
    QStringList      mimeTypes() const override;
    QMimeData*       mimeData(const QModelIndexList& indexes) const override;
    bool             dropMimeData(const QMimeData* data, Qt::DropAction action,
                                  int row, int column,
                                  const QModelIndex& parent) override;

    // Helpers
    void        reload();
    QModelIndex indexForNodeId(int nodeId) const;
    NodeData    nodeAt(const QModelIndex& index) const;

    // CRUD operations (call DB, then update model)
    bool addNode(NodeData& node, const QModelIndex& parentIndex);
    bool removeNode(const QModelIndex& index);
    bool renameNode(const QModelIndex& index, const QString& newName);
    bool updateNodeData(const NodeData& node);

signals:
    void moveRejected(const QString& reason);

private:
    void loadChildren(TreeItem* parent, int parentId);
    TreeItem* itemFromIndex(const QModelIndex& index) const;

    TreeItem* m_root;
    int       m_userId;
    bool      m_isAdmin;
};
