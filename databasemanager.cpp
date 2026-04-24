#include "databasemanager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDebug>
#include <QUuid>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{}

void DatabaseManager::setCurrentUser(int userId, bool isAdmin)
{
    m_currentUserId      = userId;
    m_currentUserIsAdmin = isAdmin;
}

// ---------------------------------------------------------------------------
// Open / init
// ---------------------------------------------------------------------------

bool DatabaseManager::openDatabase(const QString& path)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    // Enable foreign keys
    QSqlQuery q(m_db);
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("PRAGMA journal_mode = WAL");

    if (!initSchema()) return false;
    createDefaultAdmin();
    return true;
}

void DatabaseManager::closeDatabase()
{
    m_db.close();
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::initSchema()
{
    QSqlQuery q(m_db);

    const QStringList ddl = {
        R"(CREATE TABLE IF NOT EXISTS groups (
            id   INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL
        ))",
        R"(CREATE TABLE IF NOT EXISTS users (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            username      TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            is_admin      INTEGER DEFAULT 0,
            group_id      INTEGER REFERENCES groups(id) ON DELETE SET NULL
        ))",
        R"(CREATE TABLE IF NOT EXISTS nodes (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_id    INTEGER REFERENCES nodes(id) ON DELETE CASCADE,
            name         TEXT NOT NULL,
            is_directory INTEGER DEFAULT 0,
            owner_id     INTEGER REFERENCES users(id) ON DELETE SET NULL,
            attributes   TEXT DEFAULT '',
            created_at   DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at   DATETIME DEFAULT CURRENT_TIMESTAMP
        ))",
        R"(CREATE TABLE IF NOT EXISTS node_content (
            node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
            content TEXT DEFAULT ''
        ))",
        R"(CREATE TABLE IF NOT EXISTS access_rights (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id             INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
            user_id             INTEGER REFERENCES users(id) ON DELETE CASCADE,
            group_id            INTEGER REFERENCES groups(id) ON DELETE CASCADE,
            can_see             INTEGER DEFAULT 1,
            can_read            INTEGER DEFAULT 1,
            can_write           INTEGER DEFAULT 0,
            can_create_children INTEGER DEFAULT 0
        ))"
    };

    for (const QString& sql : ddl) {
        if (!q.exec(sql)) {
            m_lastError = q.lastError().text();
            qWarning() << "DDL error:" << m_lastError;
            return false;
        }
    }

    // Migration: if nodes table still has a 'content' column, move data to node_content
    QSqlQuery chk(m_db);
    chk.exec("PRAGMA table_info(nodes)");
    bool hasContentCol = false;
    while (chk.next()) {
        if (chk.value(1).toString() == "content") { hasContentCol = true; break; }
    }
    if (hasContentCol) {
        q.exec("INSERT OR IGNORE INTO node_content (node_id, content) "
               "SELECT id, content FROM nodes WHERE content IS NOT NULL AND content != ''");
        // SQLite <3.35 cannot DROP COLUMN — orphaned column is harmless
    }

    return true;
}

bool DatabaseManager::createDefaultAdmin()
{
    // Only create if no users exist
    QSqlQuery q(m_db);
    q.exec("SELECT COUNT(*) FROM users");
    if (q.next() && q.value(0).toInt() > 0) return true;

    UserData admin;
    admin.username = "admin";
    admin.isAdmin  = true;
    admin.groupId  = -1;
    return createUser(admin, "admin");
}

// ---------------------------------------------------------------------------
// Password hashing
// ---------------------------------------------------------------------------

QString DatabaseManager::hashPassword(const QString& password)
{
    // Simple SHA-256 hash (add a fixed salt prefix for slightly more security)
    QByteArray salted = "TreeNoteSalt:" + password.toUtf8();
    return QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex();
}

// ---------------------------------------------------------------------------
// Authentication
// ---------------------------------------------------------------------------

bool DatabaseManager::authenticateUser(const QString& username,
                                        const QString& password,
                                        UserData& outUser)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id, username, password_hash, is_admin, group_id "
              "FROM users WHERE username = ?");
    q.addBindValue(username);
    if (!q.exec() || !q.next()) return false;

    QString storedHash = q.value(2).toString();
    if (storedHash != hashPassword(password)) return false;

    outUser.id           = q.value(0).toInt();
    outUser.username     = q.value(1).toString();
    outUser.passwordHash = storedHash;
    outUser.isAdmin      = q.value(3).toBool();
    outUser.groupId      = q.value(4).isNull() ? -1 : q.value(4).toInt();
    return true;
}

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

QList<UserData> DatabaseManager::getUsers()
{
    QList<UserData> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id, username, password_hash, is_admin, group_id FROM users ORDER BY username");
    while (q.next()) {
        UserData u;
        u.id           = q.value(0).toInt();
        u.username     = q.value(1).toString();
        u.passwordHash = q.value(2).toString();
        u.isAdmin      = q.value(3).toBool();
        u.groupId      = q.value(4).isNull() ? -1 : q.value(4).toInt();
        list << u;
    }
    return list;
}

UserData DatabaseManager::getUser(int userId)
{
    UserData u;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, username, password_hash, is_admin, group_id "
              "FROM users WHERE id = ?");
    q.addBindValue(userId);
    if (q.exec() && q.next()) {
        u.id           = q.value(0).toInt();
        u.username     = q.value(1).toString();
        u.passwordHash = q.value(2).toString();
        u.isAdmin      = q.value(3).toBool();
        u.groupId      = q.value(4).isNull() ? -1 : q.value(4).toInt();
    }
    return u;
}

bool DatabaseManager::createUser(const UserData& user, const QString& plainPassword)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users (username, password_hash, is_admin, group_id) "
              "VALUES (?, ?, ?, ?)");
    q.addBindValue(user.username);
    q.addBindValue(hashPassword(plainPassword));
    q.addBindValue(user.isAdmin ? 1 : 0);
    q.addBindValue(user.groupId > 0 ? QVariant(user.groupId) : QVariant());
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateUser(const UserData& user)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET username=?, is_admin=?, group_id=? WHERE id=?");
    q.addBindValue(user.username);
    q.addBindValue(user.isAdmin ? 1 : 0);
    q.addBindValue(user.groupId > 0 ? QVariant(user.groupId) : QVariant());
    q.addBindValue(user.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateUserPassword(int userId, const QString& newPassword)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET password_hash=? WHERE id=?");
    q.addBindValue(hashPassword(newPassword));
    q.addBindValue(userId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteUser(int userId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM users WHERE id=?");
    q.addBindValue(userId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Groups
// ---------------------------------------------------------------------------

QList<GroupData> DatabaseManager::getGroups()
{
    QList<GroupData> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id, name FROM groups ORDER BY name");
    while (q.next()) {
        GroupData g;
        g.id   = q.value(0).toInt();
        g.name = q.value(1).toString();
        list << g;
    }
    return list;
}

bool DatabaseManager::createGroup(const GroupData& group)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO groups (name) VALUES (?)");
    q.addBindValue(group.name);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateGroup(const GroupData& group)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE groups SET name=? WHERE id=?");
    q.addBindValue(group.name);
    q.addBindValue(group.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteGroup(int groupId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM groups WHERE id=?");
    q.addBindValue(groupId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Node helpers
// ---------------------------------------------------------------------------

static NodeData nodeFromQuery(QSqlQuery& q)
{
    NodeData n;
    n.id          = q.value(0).toInt();
    n.parentId    = q.value(1).isNull() ? -1 : q.value(1).toInt();
    n.name        = q.value(2).toString();
    n.isDirectory = q.value(3).toBool();
    n.ownerId     = q.value(4).isNull() ? -1 : q.value(4).toInt();
    n.attributes  = q.value(5).toString();
    // content is loaded separately via getNodeContent()
    return n;
}

static const char* NODE_SELECT =
    "SELECT id, parent_id, name, is_directory, owner_id, attributes FROM nodes";

// ---------------------------------------------------------------------------
// Nodes
// ---------------------------------------------------------------------------

QList<NodeData> DatabaseManager::getChildren(int parentId, int userId, bool isAdmin)
{
    QList<NodeData> list;
    QSqlQuery q(m_db);

    if (parentId < 0) {
        q.prepare(QString("%1 WHERE parent_id IS NULL ORDER BY is_directory DESC, name").arg(NODE_SELECT));
    } else {
        q.prepare(QString("%1 WHERE parent_id=? ORDER BY is_directory DESC, name").arg(NODE_SELECT));
        q.addBindValue(parentId);
    }

    if (!q.exec()) return list;

    while (q.next()) {
        NodeData n = nodeFromQuery(q);
        list << n;
    }

    if (isAdmin) return list;

    // Filter by access for non-admin
    QList<NodeData> visible;
    for (const NodeData& n : list) {
        if (hasRight(n.id, userId, false, "can_see"))
            visible << n;
    }
    return visible;
}

NodeData DatabaseManager::getNode(int nodeId)
{
    if (nodeId <= 0) return {};
    if (!hasRight(nodeId, m_currentUserId, m_currentUserIsAdmin, "can_see")) {
        m_lastError = tr("Отказано в доступе: нет права видеть этот узел");
        return {};
    }

    return getNodeUnchecked(nodeId);
}

NodeData DatabaseManager::getNodeUnchecked(int nodeId)
{
    QSqlQuery q(m_db);
    q.prepare(QString("%1 WHERE id=?").arg(NODE_SELECT));
    q.addBindValue(nodeId);
    if (q.exec() && q.next()) return nodeFromQuery(q);
    return {};
}

QList<int> DatabaseManager::getSubtreeNodeIds(int rootNodeId)
{
    QList<int> ids;
    if (rootNodeId <= 0) return ids;

    QSqlQuery q(m_db);
    q.prepare(
        "WITH RECURSIVE subtree(id) AS ("
        "    SELECT id FROM nodes WHERE id = ? "
        "    UNION ALL "
        "    SELECT n.id FROM nodes n "
        "    JOIN subtree s ON n.parent_id = s.id"
        ") "
        "SELECT id FROM subtree");
    q.addBindValue(rootNodeId);
    if (!q.exec()) return ids;

    while (q.next())
        ids << q.value(0).toInt();
    return ids;
}

bool DatabaseManager::createNode(NodeData& node)
{
    // Enforce: user needs can_create_children on the parent directory
    if (node.parentId > 0) {
        if (!hasRight(node.parentId, m_currentUserId, m_currentUserIsAdmin, "can_create_children")) {
            m_lastError = tr("Отказано в доступе: нет права создавать дочерние узлы в этом каталоге");
            return false;
        }
    }

    m_db.transaction();

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO nodes (parent_id, name, is_directory, owner_id, attributes) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(node.parentId > 0 ? QVariant(node.parentId) : QVariant());
    q.addBindValue(node.name);
    q.addBindValue(node.isDirectory ? 1 : 0);
    q.addBindValue(node.ownerId > 0 ? QVariant(node.ownerId) : QVariant());
    q.addBindValue(node.attributes);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        m_db.rollback();
        return false;
    }
    node.id = q.lastInsertId().toInt();

    // Create empty content record in the separate node_content table
    QSqlQuery qc(m_db);
    qc.prepare("INSERT INTO node_content (node_id, content) VALUES (?, '')");
    qc.addBindValue(node.id);
    if (!qc.exec()) {
        m_lastError = qc.lastError().text();
        m_db.rollback();
        return false;
    }

    m_db.commit();
    return true;
}

bool DatabaseManager::updateNode(const NodeData& node)
{
    if (!hasRight(node.id, m_currentUserId, m_currentUserIsAdmin, "can_write")) {
        m_lastError = tr("Отказано в доступе: нет права изменять этот узел");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE nodes SET name=?, is_directory=?, attributes=?, "
              "updated_at=CURRENT_TIMESTAMP WHERE id=?");
    q.addBindValue(node.name);
    q.addBindValue(node.isDirectory ? 1 : 0);
    q.addBindValue(node.attributes);
    q.addBindValue(node.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteNode(int nodeId)
{
    if (!hasRight(nodeId, m_currentUserId, m_currentUserIsAdmin, "can_write")) {
        m_lastError = tr("Отказано в доступе: нет права удалять этот узел");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM nodes WHERE id=?");
    q.addBindValue(nodeId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::moveNode(int nodeId, int newParentId)
{
    if (nodeId == newParentId) return false;

    // Cycle prevention: newParentId must not be a descendant of nodeId
    if (newParentId > 0 && isAncestorOf(nodeId, newParentId)) {
        m_lastError = tr("Нельзя переместить узел в собственное поддерево");
        return false;
    }

    // Permission: user must have can_write on the node being moved
    if (!hasRight(nodeId, m_currentUserId, m_currentUserIsAdmin, "can_write")) {
        m_lastError = tr("Отказано в доступе: нет права перемещать этот узел");
        return false;
    }

    // Permission: user must have can_create_children on the target parent
    if (newParentId > 0 &&
        !hasRight(newParentId, m_currentUserId, m_currentUserIsAdmin, "can_create_children")) {
        m_lastError = tr("Отказано в доступе: нет права создавать дочерние узлы в целевом каталоге");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE nodes SET parent_id=? WHERE id=?");
    q.addBindValue(newParentId > 0 ? QVariant(newParentId) : QVariant());
    q.addBindValue(nodeId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

QString DatabaseManager::getNodeContent(int nodeId)
{
    if (nodeId <= 0) return {};
    if (!hasRight(nodeId, m_currentUserId, m_currentUserIsAdmin, "can_read")) {
        m_lastError = tr("Отказано в доступе: нет права читать содержимое этого узла");
        return {};
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT content FROM node_content WHERE node_id=?");
    q.addBindValue(nodeId);
    if (q.exec() && q.next()) return q.value(0).toString();
    return {};
}

bool DatabaseManager::setNodeContent(int nodeId, const QString& content)
{
    if (!hasRight(nodeId, m_currentUserId, m_currentUserIsAdmin, "can_write")) {
        m_lastError = tr("Отказано в доступе: нет права изменять содержимое этого узла");
        return false;
    }

    QSqlQuery q(m_db);
    // INSERT OR REPLACE works as upsert for the separate content table
    q.prepare("INSERT OR REPLACE INTO node_content (node_id, content) VALUES (?, ?)");
    q.addBindValue(nodeId);
    q.addBindValue(content);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Access rights
// ---------------------------------------------------------------------------

bool DatabaseManager::isAncestorOf(int ancestorId, int nodeId)
{
    // Walk up from nodeId; return true if we encounter ancestorId
    int current = nodeId;
    while (current > 0) {
        QSqlQuery q(m_db);
        q.prepare("SELECT parent_id FROM nodes WHERE id=?");
        q.addBindValue(current);
        if (!q.exec() || !q.next()) break;
        QVariant pv = q.value(0);
        if (pv.isNull()) break;
        current = pv.toInt();
        if (current == ancestorId) return true;
    }
    return false;
}

bool DatabaseManager::hasRight(int nodeId, int userId, bool isAdmin, const QString& right)
{
    if (isAdmin) return true;

    // Owner has all rights
    QSqlQuery own(m_db);
    own.prepare("SELECT owner_id, parent_id FROM nodes WHERE id=?");
    own.addBindValue(nodeId);
    if (own.exec() && own.next()) {
        if (!own.value(0).isNull() && own.value(0).toInt() == userId) return true;
    }

    // Check whether explicit rules exist for this node
    QSqlQuery cnt(m_db);
    cnt.prepare("SELECT COUNT(*) FROM access_rights WHERE node_id=?");
    cnt.addBindValue(nodeId);
    cnt.exec(); cnt.next();

    if (cnt.value(0).toInt() == 0) {
        // No explicit rules → inherit from parent directory
        QSqlQuery pq(m_db);
        pq.prepare("SELECT parent_id FROM nodes WHERE id=?");
        pq.addBindValue(nodeId);
        if (pq.exec() && pq.next() && !pq.value(0).isNull()) {
            return hasRight(pq.value(0).toInt(), userId, isAdmin, right);
        }
        // Root level, no rules → allow read/see by default
        return (right == "can_see" || right == "can_read");
    }

    // Check user-specific rule
    QSqlQuery uq(m_db);
    uq.prepare(QString("SELECT %1 FROM access_rights WHERE node_id=? AND user_id=?").arg(right));
    uq.addBindValue(nodeId);
    uq.addBindValue(userId);
    if (uq.exec() && uq.next()) return uq.value(0).toBool();

    // Check group rule
    QSqlQuery gq(m_db);
    gq.prepare(QString("SELECT ar.%1 FROM access_rights ar "
                       "JOIN users u ON u.group_id = ar.group_id "
                       "WHERE ar.node_id=? AND u.id=?").arg(right));
    gq.addBindValue(nodeId);
    gq.addBindValue(userId);
    if (gq.exec() && gq.next()) return gq.value(0).toBool();

    // Explicit rules exist for node but none match this user → deny
    return false;
}

QList<AccessRightData> DatabaseManager::getAccessRights(int nodeId)
{
    QList<AccessRightData> list;
    NodeData nd = getNodeUnchecked(nodeId);
    if (nodeId > 0 && !m_currentUserIsAdmin && nd.ownerId != m_currentUserId) {
        m_lastError = tr("Отказано в доступе: только владелец или администратор может просматривать права доступа");
        return list;
    }

    QSqlQuery q(m_db);
    q.prepare("SELECT id, node_id, user_id, group_id, "
              "can_see, can_read, can_write, can_create_children "
              "FROM access_rights WHERE node_id=?");
    q.addBindValue(nodeId);
    if (!q.exec()) return list;
    while (q.next()) {
        AccessRightData a;
        a.id                = q.value(0).toInt();
        a.nodeId            = q.value(1).toInt();
        a.userId            = q.value(2).isNull() ? -1 : q.value(2).toInt();
        a.groupId           = q.value(3).isNull() ? -1 : q.value(3).toInt();
        a.canSee            = q.value(4).toBool();
        a.canRead           = q.value(5).toBool();
        a.canWrite          = q.value(6).toBool();
        a.canCreateChildren = q.value(7).toBool();
        list << a;
    }
    return list;
}

bool DatabaseManager::setAccessRight(const AccessRightData& ar)
{
    // Only the node owner or admin may set access rights
    NodeData nd = getNodeUnchecked(ar.nodeId);
    if (!m_currentUserIsAdmin && nd.ownerId != m_currentUserId) {
        m_lastError = tr("Отказано в доступе: только владелец или администратор может управлять правами");
        return false;
    }

    QSqlQuery q(m_db);
    if (ar.id > 0) {
        q.prepare("UPDATE access_rights SET can_see=?, can_read=?, can_write=?, "
                  "can_create_children=? WHERE id=?");
        q.addBindValue(ar.canSee ? 1 : 0);
        q.addBindValue(ar.canRead ? 1 : 0);
        q.addBindValue(ar.canWrite ? 1 : 0);
        q.addBindValue(ar.canCreateChildren ? 1 : 0);
        q.addBindValue(ar.id);
    } else {
        q.prepare("INSERT INTO access_rights "
                  "(node_id, user_id, group_id, can_see, can_read, can_write, can_create_children) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(ar.nodeId);
        q.addBindValue(ar.userId  > 0 ? QVariant(ar.userId)  : QVariant());
        q.addBindValue(ar.groupId > 0 ? QVariant(ar.groupId) : QVariant());
        q.addBindValue(ar.canSee ? 1 : 0);
        q.addBindValue(ar.canRead ? 1 : 0);
        q.addBindValue(ar.canWrite ? 1 : 0);
        q.addBindValue(ar.canCreateChildren ? 1 : 0);
    }
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::setAccessRightRecursive(const AccessRightData& ar)
{
    NodeData rootNode = getNodeUnchecked(ar.nodeId);
    if (!m_currentUserIsAdmin && rootNode.ownerId != m_currentUserId) {
        m_lastError = tr("Отказано в доступе: только владелец каталога или администратор может применять права ко всему поддереву");
        return false;
    }

    const QList<int> nodeIds = getSubtreeNodeIds(ar.nodeId);
    if (nodeIds.isEmpty()) {
        m_lastError = tr("Не удалось определить поддерево для применения прав");
        return false;
    }

    m_db.transaction();

    for (int nodeId : nodeIds) {
        QSqlQuery q(m_db);
        q.prepare(
            "SELECT id FROM access_rights "
            "WHERE node_id=? AND "
            "((user_id IS NOT NULL AND user_id=?) OR (group_id IS NOT NULL AND group_id=?))");
        q.addBindValue(nodeId);
        q.addBindValue(ar.userId > 0 ? QVariant(ar.userId) : QVariant());
        q.addBindValue(ar.groupId > 0 ? QVariant(ar.groupId) : QVariant());

        AccessRightData copy = ar;
        copy.nodeId = nodeId;

        if (q.exec() && q.next())
            copy.id = q.value(0).toInt();
        else
            copy.id = -1;

        QSqlQuery write(m_db);
        if (copy.id > 0) {
            write.prepare("UPDATE access_rights SET can_see=?, can_read=?, can_write=?, "
                          "can_create_children=? WHERE id=?");
            write.addBindValue(copy.canSee ? 1 : 0);
            write.addBindValue(copy.canRead ? 1 : 0);
            write.addBindValue(copy.canWrite ? 1 : 0);
            write.addBindValue(copy.canCreateChildren ? 1 : 0);
            write.addBindValue(copy.id);
        } else {
            write.prepare("INSERT INTO access_rights "
                          "(node_id, user_id, group_id, can_see, can_read, can_write, can_create_children) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?)");
            write.addBindValue(copy.nodeId);
            write.addBindValue(copy.userId > 0 ? QVariant(copy.userId) : QVariant());
            write.addBindValue(copy.groupId > 0 ? QVariant(copy.groupId) : QVariant());
            write.addBindValue(copy.canSee ? 1 : 0);
            write.addBindValue(copy.canRead ? 1 : 0);
            write.addBindValue(copy.canWrite ? 1 : 0);
            write.addBindValue(copy.canCreateChildren ? 1 : 0);
        }

        if (!write.exec()) {
            m_lastError = write.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    m_db.commit();
    return true;
}

bool DatabaseManager::deleteAccessRight(int rightId)
{
    // Resolve owning node and check permission
    QSqlQuery nq(m_db);
    nq.prepare("SELECT node_id FROM access_rights WHERE id=?");
    nq.addBindValue(rightId);
    if (nq.exec() && nq.next()) {
        NodeData nd = getNodeUnchecked(nq.value(0).toInt());
        if (!m_currentUserIsAdmin && nd.ownerId != m_currentUserId) {
            m_lastError = tr("Отказано в доступе: только владелец или администратор может удалять права");
            return false;
        }
    }

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM access_rights WHERE id=?");
    q.addBindValue(rightId);
    return q.exec();
}

bool DatabaseManager::deleteAccessRightsForNode(int nodeId)
{
    NodeData nd = getNodeUnchecked(nodeId);
    if (nodeId > 0 && !m_currentUserIsAdmin && nd.ownerId != m_currentUserId) {
        m_lastError = tr("Отказано в доступе: только владелец или администратор может удалять права доступа");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM access_rights WHERE node_id=?");
    q.addBindValue(nodeId);
    return q.exec();
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

QList<NodeData> DatabaseManager::searchByName(const QString& query, int userId, bool isAdmin)
{
    QList<NodeData> list;
    QSqlQuery q(m_db);
    q.prepare(QString("%1 WHERE name LIKE ? ORDER BY name").arg(NODE_SELECT));
    q.addBindValue("%" + query + "%");
    if (!q.exec()) return list;
    while (q.next()) list << nodeFromQuery(q);
    if (isAdmin) return list;
    return filterByAccess(list, userId, false);
}

QList<NodeData> DatabaseManager::searchByContent(const QString& query, int userId, bool isAdmin)
{
    QList<NodeData> list;
    QSqlQuery q(m_db);
    // JOIN with the separate node_content table
    q.prepare("SELECT n.id, n.parent_id, n.name, n.is_directory, n.owner_id, n.attributes "
              "FROM nodes n JOIN node_content nc ON nc.node_id = n.id "
              "WHERE nc.content LIKE ? ORDER BY n.name");
    q.addBindValue("%" + query + "%");
    if (!q.exec()) return list;
    while (q.next()) list << nodeFromQuery(q);
    if (isAdmin) return list;
    return filterByAccess(list, userId, false);
}

QList<NodeData> DatabaseManager::searchByAttributes(const QString& query, int userId, bool isAdmin)
{
    QList<NodeData> list;
    QSqlQuery q(m_db);
    q.prepare(QString("%1 WHERE attributes LIKE ? ORDER BY name").arg(NODE_SELECT));
    q.addBindValue("%" + query + "%");
    if (!q.exec()) return list;
    while (q.next()) list << nodeFromQuery(q);
    if (isAdmin) return list;
    return filterByAccess(list, userId, false);
}

QList<NodeData> DatabaseManager::filterByAccess(QList<NodeData> nodes, int userId, bool isAdmin)
{
    QList<NodeData> result;
    for (const NodeData& n : nodes) {
        if (hasRight(n.id, userId, isAdmin, "can_see") &&
            hasRight(n.id, userId, isAdmin, "can_read"))
            result << n;
    }
    return result;
}
