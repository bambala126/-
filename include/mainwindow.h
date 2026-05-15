#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TreeModel;
class QTextEdit;
class QLabel;

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const UserData& user, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // Tree
    void onTreeSelectionChanged(const QModelIndex& current,
                                const QModelIndex& previous);
    void onTreeContextMenu(const QPoint& pos);

    // Node actions
    void slotNewNode();
    void slotNewDirectory();
    void slotEditNode();
    void slotDeleteNode();
    void slotMoveNodeUp();
    void slotMoveNodeDown();
    void slotAccessRights();
    void slotReloadTree();

    // Content
    void slotSaveContent();
    void slotLoadFromFile();
    void slotSaveToFile();
    void slotContentChanged();

    // Tabs
    void slotTabCloseRequested(int index);
    void slotOpenNodeInTab(const QModelIndex& index);

    // Search & admin
    void slotSearch();
    void slotAdminPanel();

    // Status
    void updateStatusBar();

private:
    void setupActions();
    void setupToolBar();
    void setupMenuBar();
    void updateActions(const QModelIndex& index);

    // Tab helpers
    int  findTabForNode(int nodeId) const;
    void openTab(const NodeData& node);
    void saveCurrentTab();

    Ui::MainWindow* ui;
    TreeModel*      m_model;
    UserData        m_currentUser;

    // Per-tab tracking: tab-index → nodeId, isDirty
    QMap<int, int>  m_tabNodeIds;   // tabIndex → nodeId
    QMap<int, bool> m_tabDirty;

    // Actions
    QAction* m_actNewNode;
    QAction* m_actNewDir;
    QAction* m_actEdit;
    QAction* m_actDelete;
    QAction* m_actMoveUp;
    QAction* m_actMoveDown;
    QAction* m_actAccessRights;
    QAction* m_actSave;
    QAction* m_actLoadFile;
    QAction* m_actSaveFile;
    QAction* m_actSearch;
    QAction* m_actAdmin;
    QAction* m_actReload;

    QLabel* m_statusLabel;
};
