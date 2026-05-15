#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QList>

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct NodeData {
    int     id          = -1;
    int     parentId    = -1;
    QString name;
    bool    isDirectory = false;
    int     ownerId     = -1;
    QString attributes;
    QString content;

    bool operator==(const NodeData& o) const { return id == o.id; }
};

struct UserData {
    int     id       = -1;
    QString username;
    QString passwordHash;
    bool    isAdmin  = false;
    int     groupId  = -1;
};

struct GroupData {
    int     id = -1;
    QString name;
};

struct AccessRightData {
    int  id                = -1;
    int  nodeId            = -1;
    int  userId            = -1;   // -1 → not a user rule
    int  groupId           = -1;   // -1 → not a group rule
    bool canSee            = true;
    bool canRead           = true;
    bool canWrite          = false;
    bool canCreateChildren = false;
};

// ---------------------------------------------------------------------------
// DatabaseManager — singleton
// ---------------------------------------------------------------------------

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool openDatabase(const QString& path);
    void closeDatabase();
    bool isOpen() const;

    // ---- Session (call after login) -------------------------------------
    void setCurrentUser(int userId, bool isAdmin);
    int  currentUserId()    const { return m_currentUserId; }
    bool currentUserIsAdmin() const { return m_currentUserIsAdmin; }

    // ---- Authentication -------------------------------------------------
    bool    authenticateUser(const QString& username,
                             const QString& password,
                             UserData& outUser);
    QString hashPassword(const QString& password);

    // ---- Users ----------------------------------------------------------
    QList<UserData> getUsers();
    bool createUser(const UserData& user, const QString& plainPassword);
    bool updateUser(const UserData& user);
    bool updateUserPassword(int userId, const QString& newPassword);
    bool deleteUser(int userId);
    UserData getUser(int userId);

    // ---- Groups ---------------------------------------------------------
    QList<GroupData> getGroups();
    bool createGroup(const GroupData& group);
    bool updateGroup(const GroupData& group);
    bool deleteGroup(int groupId);

    // ---- Nodes (enforce permissions internally via current user) ---------
    QList<NodeData> getChildren(int parentId, int userId, bool isAdmin);
    NodeData        getNode(int nodeId);
    bool            createNode(NodeData& node);   // sets node.id on success
    bool            updateNode(const NodeData& node);
    bool            deleteNode(int nodeId);
    bool            moveNode(int nodeId, int newParentId);

    // ---- Content --------------------------------------------------------
    QString getNodeContent(int nodeId);
    bool    setNodeContent(int nodeId, const QString& content);

    // ---- Access rights --------------------------------------------------
    bool hasRight(int nodeId, int userId, bool isAdmin, const QString& right);
    QList<AccessRightData> getAccessRights(int nodeId);
    bool setAccessRight(const AccessRightData& right);
    bool setAccessRightRecursive(const AccessRightData& right);
    bool deleteAccessRight(int rightId);
    bool deleteAccessRightsForNode(int nodeId);

    // ---- Search ---------------------------------------------------------
    QList<NodeData> searchByName      (const QString& q, int userId, bool isAdmin);
    QList<NodeData> searchByContent   (const QString& q, int userId, bool isAdmin);
    QList<NodeData> searchByAttributes(const QString& q, int userId, bool isAdmin);

    QString lastError() const { return m_lastError; }

private:
    explicit DatabaseManager(QObject* parent = nullptr);

    bool initSchema();
    bool createDefaultAdmin();
    QList<NodeData> filterByAccess(QList<NodeData> nodes, int userId, bool isAdmin);
    NodeData getNodeUnchecked(int nodeId);
    QList<int> getSubtreeNodeIds(int rootNodeId);

    // Returns true if ancestorId is a strict ancestor of nodeId in the tree
    bool isAncestorOf(int ancestorId, int nodeId);

    QSqlDatabase m_db;
    QString      m_lastError;

    int  m_currentUserId      = -1;
    bool m_currentUserIsAdmin = false;
};
