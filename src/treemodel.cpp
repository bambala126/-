#include "treemodel.h"
#include "databasemanager.h"

#include <QMimeData>
#include <QDataStream>
#include <QIODevice>
#include <QIcon>

// ---------------------------------------------------------------------------
// TreeItem
// ---------------------------------------------------------------------------

TreeItem::TreeItem(const NodeData& data, TreeItem* parent)
    : m_data(data), m_parent(parent)
{}

TreeItem::~TreeItem()
{
    qDeleteAll(m_children);
}

void TreeItem::appendChild(TreeItem* child)
{
    m_children.append(child);
}

void TreeItem::removeChild(int row)
{
    if (row >= 0 && row < m_children.size())
        delete m_children.takeAt(row);
}

void TreeItem::insertChild(int row, TreeItem* child)
{
    m_children.insert(row, child);
}

TreeItem* TreeItem::child(int row)
{
    return m_children.value(row, nullptr);
}

TreeItem* TreeItem::parentItem()
{
    return m_parent;
}

int TreeItem::childCount() const
{
    return m_children.size();
}

int TreeItem::row() const
{
    if (m_parent)
        return m_parent->m_children.indexOf(const_cast<TreeItem*>(this));
    return 0;
}

NodeData& TreeItem::data() { return m_data; }
const NodeData& TreeItem::data() const { return m_data; }

// ---------------------------------------------------------------------------
// TreeModel
// ---------------------------------------------------------------------------

TreeModel::TreeModel(int userId, bool isAdmin, QObject* parent)
    : QAbstractItemModel(parent), m_userId(userId), m_isAdmin(isAdmin)
{
    NodeData root;
    root.id   = -1;
    root.name = "root";
    m_root = new TreeItem(root);
    loadChildren(m_root, -1);
}

TreeModel::~TreeModel()
{
    delete m_root;
}

void TreeModel::loadChildren(TreeItem* parent, int parentId)
{
    auto& db = DatabaseManager::instance();
    QList<NodeData> children = db.getChildren(parentId, m_userId, m_isAdmin);
    for (const NodeData& nd : children) {
        TreeItem* item = new TreeItem(nd, parent);
        parent->appendChild(item);
        if (nd.isDirectory)
            loadChildren(item, nd.id);
    }
}

void TreeModel::reload()
{
    beginResetModel();
    delete m_root;
    NodeData root;
    root.id   = -1;
    root.name = "root";
    m_root = new TreeItem(root);
    loadChildren(m_root, -1);
    endResetModel();
}

// ---------------------------------------------------------------------------
// Core interface
// ---------------------------------------------------------------------------

QModelIndex TreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) return {};
    TreeItem* parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_root;
    TreeItem* child = parentItem->child(row);
    return child ? createIndex(row, column, child) : QModelIndex{};
}

QModelIndex TreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return {};
    TreeItem* item   = static_cast<TreeItem*>(child.internalPointer());
    TreeItem* parentItem = item->parentItem();
    if (parentItem == m_root) return {};
    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* p = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : m_root;
    return p->childCount();
}

int TreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant TreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    const NodeData& nd = item->data();

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return nd.name;
    case Qt::DecorationRole:
        if (nd.isDirectory)
            return QIcon::fromTheme("folder", QIcon(":/icons/folder.png"));
        return QIcon::fromTheme("text-x-generic", QIcon(":/icons/file.png"));
    case Qt::UserRole:
        return nd.id;
    case Qt::UserRole + 1:
        return nd.isDirectory;
    case Qt::ToolTipRole:
        return nd.attributes.isEmpty()
               ? nd.name
               : QString("%1\n%2").arg(nd.name, nd.attributes);
    default:
        return {};
    }
}

bool TreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    QString newName = value.toString().trimmed();
    if (newName.isEmpty()) return false;
    item->data().name = newName;
    if (!DatabaseManager::instance().updateNode(item->data())) return false;
    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags TreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable |
           Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
        return tr("Узлы");
    return {};
}

// ---------------------------------------------------------------------------
// Drag & drop
// ---------------------------------------------------------------------------

Qt::DropActions TreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList TreeModel::mimeTypes() const
{
    return {"application/treenote.nodeid"};
}

QMimeData* TreeModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty()) return nullptr;
    auto* mime = new QMimeData;
    QByteArray enc;
    QDataStream ds(&enc, QIODevice::WriteOnly);
    for (const QModelIndex& idx : indexes) {
        if (idx.isValid())
            ds << idx.data(Qt::UserRole).toInt();
    }
    mime->setData("application/treenote.nodeid", enc);
    return mime;
}

bool TreeModel::dropMimeData(const QMimeData* mdata, Qt::DropAction action,
                              int /*row*/, int /*column*/, const QModelIndex& parent)
{
    if (action != Qt::MoveAction) return false;
    if (!mdata->hasFormat("application/treenote.nodeid")) return false;

    QByteArray enc = mdata->data("application/treenote.nodeid");
    QDataStream ds(&enc, QIODevice::ReadOnly);

    auto& db = DatabaseManager::instance();
    bool anyMoved = false;

    while (!ds.atEnd()) {
        int nodeId;
        ds >> nodeId;
        int newParentId = parent.isValid() ? parent.data(Qt::UserRole).toInt() : -1;

        // moveNode() now validates: cycle prevention + can_write + can_create_children
        if (db.moveNode(nodeId, newParentId)) {
            anyMoved = true;
        } else {
            // Emit a signal so MainWindow can display the error
            emit moveRejected(db.lastError());
        }
    }

    if (anyMoved) reload();
    return anyMoved;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

TreeItem* TreeModel::itemFromIndex(const QModelIndex& index) const
{
    return index.isValid()
        ? static_cast<TreeItem*>(index.internalPointer())
        : m_root;
}

NodeData TreeModel::nodeAt(const QModelIndex& index) const
{
    return itemFromIndex(index)->data();
}

QModelIndex TreeModel::indexForNodeId(int nodeId) const
{
    std::function<QModelIndex(TreeItem*, const QModelIndex&)> find;
    find = [&](TreeItem* item, const QModelIndex& parentIdx) -> QModelIndex {
        for (int r = 0; r < item->childCount(); ++r) {
            TreeItem* ch = item->child(r);
            if (ch->data().id == nodeId) return index(r, 0, parentIdx);
            QModelIndex sub = index(r, 0, parentIdx);
            QModelIndex found = find(ch, sub);
            if (found.isValid()) return found;
        }
        return {};
    };
    return find(m_root, {});
}

// ---------------------------------------------------------------------------
// CRUD via model
// ---------------------------------------------------------------------------

bool TreeModel::addNode(NodeData& node, const QModelIndex& parentIndex)
{
    if (!DatabaseManager::instance().createNode(node)) return false;

    TreeItem* parentItem = itemFromIndex(parentIndex);
    int row = parentItem->childCount();
    beginInsertRows(parentIndex, row, row);
    parentItem->appendChild(new TreeItem(node, parentItem));
    endInsertRows();
    return true;
}

bool TreeModel::removeNode(const QModelIndex& index)
{
    if (!index.isValid()) return false;
    TreeItem* item = itemFromIndex(index);
    if (!DatabaseManager::instance().deleteNode(item->data().id)) return false;

    QModelIndex parentIdx = parent(index);
    TreeItem*   parentItem = itemFromIndex(parentIdx);
    int row = item->row();
    beginRemoveRows(parentIdx, row, row);
    parentItem->removeChild(row);
    endRemoveRows();
    return true;
}

bool TreeModel::renameNode(const QModelIndex& index, const QString& newName)
{
    return setData(index, newName, Qt::EditRole);
}

bool TreeModel::updateNodeData(const NodeData& node)
{
    if (!DatabaseManager::instance().updateNode(node)) return false;
    QModelIndex idx = indexForNodeId(node.id);
    if (idx.isValid()) {
        itemFromIndex(idx)->data() = node;
        emit dataChanged(idx, idx);
    }
    return true;
}
