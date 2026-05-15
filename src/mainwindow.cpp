#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "treemodel.h"
#include "nodedialog.h"
#include "admindialog.h"
#include "accessrightsdialog.h"
#include "searchdialog.h"
#include "databasemanager.h"

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QSplitter>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QCloseEvent>
#include <QFile>
#include <QTextStream>
#include <QTextBlock>
#include <QTextCursor>
#include <QMap>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(const UserData& user, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentUser(user)
{
    ui->setupUi(this);

    setWindowTitle(tr("Древовидный блокнот — %1").arg(user.username));

    // Set splitter initial proportions: 30% tree / 70% editor
    ui->splitterMain->setSizes({300, 700});

    m_model = new TreeModel(user.id, user.isAdmin, this);
    ui->treeView->setModel(m_model);
    ui->treeView->setDragDropMode(QAbstractItemView::DragDrop);
    ui->treeView->setDefaultDropAction(Qt::MoveAction);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeView->expandAll();

    // Status bar label
    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);
    updateStatusBar();

    setupActions();
    setupMenuBar();
    setupToolBar();

    // Quick search filter on tree
    connect(ui->lineEditQuickSearch, &QLineEdit::textChanged, this, [this](const QString& text){
        // Expand all on empty, otherwise let user browse manually
        if (text.isEmpty()) {
            ui->treeView->expandAll();
        } else {
            // Simple: highlight first match
            auto list = DatabaseManager::instance().searchByName(text,
                m_currentUser.id, m_currentUser.isAdmin);
            if (!list.isEmpty()) {
                QModelIndex idx = m_model->indexForNodeId(list.first().id);
                if (idx.isValid()) {
                    ui->treeView->setCurrentIndex(idx);
                    ui->treeView->scrollTo(idx);
                }
            }
        }
    });

    connect(m_model, &TreeModel::moveRejected, this, [this](const QString& reason){
        QMessageBox::warning(this, tr("Перемещение невозможно"), reason);
    });

    connect(ui->treeView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, &MainWindow::onTreeSelectionChanged);
    connect(ui->treeView, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    connect(ui->treeView, &QTreeView::doubleClicked,
            this, &MainWindow::slotOpenNodeInTab);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::slotTabCloseRequested);

    updateActions({});
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Actions setup
// ---------------------------------------------------------------------------

void MainWindow::setupActions()
{
    // Use QStyle standard icons — work on all platforms including Windows
    auto si = [this](QStyle::StandardPixmap sp){ return style()->standardIcon(sp); };

    m_actNewNode  = new QAction(si(QStyle::SP_FileIcon),          tr("Новый узел"),         this);
    m_actNewDir   = new QAction(si(QStyle::SP_DirIcon),           tr("Новый каталог"),      this);
    m_actEdit     = new QAction(si(QStyle::SP_FileDialogDetailedView), tr("Свойства узла"), this);
    m_actDelete   = new QAction(si(QStyle::SP_TrashIcon),         tr("Удалить узел"),       this);
    m_actMoveUp   = new QAction(si(QStyle::SP_ArrowUp),           tr("Вверх"),              this);
    m_actMoveDown = new QAction(si(QStyle::SP_ArrowDown),         tr("Вниз"),               this);
    m_actAccessRights = new QAction(si(QStyle::SP_FileDialogInfoView), tr("Права доступа"), this);
    m_actSave     = new QAction(si(QStyle::SP_DialogSaveButton),  tr("Сохранить"),          this);
    m_actLoadFile = new QAction(si(QStyle::SP_DialogOpenButton),  tr("Загрузить из файла"), this);
    m_actSaveFile = new QAction(si(QStyle::SP_DriveFDIcon),       tr("Сохранить в файл"),   this);
    m_actSearch   = new QAction(si(QStyle::SP_FileDialogContentsView), tr("Поиск..."),      this);
    m_actAdmin    = new QAction(si(QStyle::SP_ComputerIcon),      tr("Администрирование"),  this);
    m_actReload   = new QAction(si(QStyle::SP_BrowserReload),     tr("Обновить дерево"),    this);

    m_actNewNode->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    m_actSave->setShortcut(QKeySequence::Save);
    m_actSearch->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    m_actDelete->setShortcut(QKeySequence::Delete);
    m_actReload->setShortcut(QKeySequence(Qt::Key_F5));

    if (!m_currentUser.isAdmin)
        m_actAdmin->setVisible(false);

    connect(m_actNewNode,      &QAction::triggered, this, &MainWindow::slotNewNode);
    connect(m_actNewDir,       &QAction::triggered, this, &MainWindow::slotNewDirectory);
    connect(m_actEdit,         &QAction::triggered, this, &MainWindow::slotEditNode);
    connect(m_actDelete,       &QAction::triggered, this, &MainWindow::slotDeleteNode);
    connect(m_actMoveUp,       &QAction::triggered, this, &MainWindow::slotMoveNodeUp);
    connect(m_actMoveDown,     &QAction::triggered, this, &MainWindow::slotMoveNodeDown);
    connect(m_actAccessRights, &QAction::triggered, this, &MainWindow::slotAccessRights);
    connect(m_actSave,         &QAction::triggered, this, &MainWindow::slotSaveContent);
    connect(m_actLoadFile,     &QAction::triggered, this, &MainWindow::slotLoadFromFile);
    connect(m_actSaveFile,     &QAction::triggered, this, &MainWindow::slotSaveToFile);
    connect(m_actSearch,       &QAction::triggered, this, &MainWindow::slotSearch);
    connect(m_actAdmin,        &QAction::triggered, this, &MainWindow::slotAdminPanel);
    connect(m_actReload,       &QAction::triggered, this, &MainWindow::slotReloadTree);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actLoadFile);
    fileMenu->addAction(m_actSaveFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Выход"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Правка"));
    editMenu->addAction(m_actNewNode);
    editMenu->addAction(m_actNewDir);
    editMenu->addSeparator();
    editMenu->addAction(m_actEdit);
    editMenu->addAction(m_actDelete);
    editMenu->addSeparator();
    editMenu->addAction(m_actMoveUp);
    editMenu->addAction(m_actMoveDown);
    editMenu->addSeparator();
    editMenu->addAction(m_actAccessRights);

    QMenu* viewMenu = menuBar()->addMenu(tr("&Вид"));
    viewMenu->addAction(m_actReload);
    viewMenu->addAction(m_actSearch);

    QMenu* adminMenu = menuBar()->addMenu(tr("&Инструменты"));
    adminMenu->addAction(m_actAdmin);
}

void MainWindow::setupToolBar()
{
    QToolBar* tb = addToolBar(tr("Основная панель"));
    tb->setObjectName("mainToolBar");
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tb->addAction(m_actNewNode);
    tb->addAction(m_actNewDir);
    tb->addSeparator();
    tb->addAction(m_actEdit);
    tb->addAction(m_actDelete);
    tb->addSeparator();
    tb->addAction(m_actSave);
    tb->addSeparator();
    tb->addAction(m_actSearch);
    if (m_currentUser.isAdmin) {
        tb->addSeparator();
        tb->addAction(m_actAdmin);
    }
    tb->addAction(m_actReload);
}

// ---------------------------------------------------------------------------
// Tree events
// ---------------------------------------------------------------------------

void MainWindow::onTreeSelectionChanged(const QModelIndex& current,
                                         const QModelIndex& /*previous*/)
{
    updateActions(current);
    if (!current.isValid()) return;
    NodeData nd = m_model->nodeAt(current);
    statusBar()->showMessage(tr("Узел: %1 (id=%2)").arg(nd.name).arg(nd.id), 3000);
}

void MainWindow::onTreeContextMenu(const QPoint& pos)
{
    QModelIndex idx = ui->treeView->indexAt(pos);
    QMenu menu(this);

    if (idx.isValid()) {
        menu.addAction(m_actNewNode);
        menu.addAction(m_actNewDir);
        menu.addSeparator();
        menu.addAction(m_actEdit);
        menu.addAction(m_actDelete);
        menu.addSeparator();
        menu.addAction(m_actAccessRights);
        menu.addSeparator();
        menu.addAction(tr("Открыть во вкладке"), this, [this, idx]{
            slotOpenNodeInTab(idx);
        });
    } else {
        menu.addAction(m_actNewNode);
        menu.addAction(m_actNewDir);
    }

    menu.exec(ui->treeView->viewport()->mapToGlobal(pos));
}

void MainWindow::updateActions(const QModelIndex& index)
{
    bool valid = index.isValid();
    bool canWrite = false;
    bool canCreate = false;

    if (valid) {
        NodeData nd = m_model->nodeAt(index);
        auto& db = DatabaseManager::instance();
        canWrite  = db.hasRight(nd.id, m_currentUser.id, m_currentUser.isAdmin, "can_write");
        canCreate = db.hasRight(nd.id, m_currentUser.id, m_currentUser.isAdmin, "can_create_children");
        // Owner always can
        if (nd.ownerId == m_currentUser.id || m_currentUser.isAdmin) {
            canWrite = canCreate = true;
        }
    } else {
        canCreate = true; // root level — allow new nodes
    }

    m_actEdit->setEnabled(valid && canWrite);
    m_actDelete->setEnabled(valid && canWrite);
    m_actMoveUp->setEnabled(valid && canWrite);
    m_actMoveDown->setEnabled(valid && canWrite);
    m_actAccessRights->setEnabled(valid && (m_currentUser.isAdmin ||
        (valid && m_model->nodeAt(index).ownerId == m_currentUser.id)));
    m_actNewNode->setEnabled(canCreate);
    m_actNewDir->setEnabled(canCreate);
}

// ---------------------------------------------------------------------------
// Node actions
// ---------------------------------------------------------------------------

void MainWindow::slotNewNode()
{
    QModelIndex parentIdx = ui->treeView->currentIndex();
    NodeData nd;
    nd.parentId = parentIdx.isValid() ? m_model->nodeAt(parentIdx).id : -1;
    nd.ownerId  = m_currentUser.id;
    nd.isDirectory = false;

    NodeDialog dlg(this);
    dlg.setNode(nd);
    dlg.setWindowTitle(tr("Новый узел"));
    if (dlg.exec() != QDialog::Accepted) return;

    nd = dlg.node();
    nd.ownerId  = m_currentUser.id;
    nd.parentId = parentIdx.isValid() ? m_model->nodeAt(parentIdx).id : -1;

    if (!m_model->addNode(nd, parentIdx)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось создать узел:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    ui->treeView->expand(parentIdx);
    statusBar()->showMessage(tr("Узел '%1' создан").arg(nd.name), 3000);
}

void MainWindow::slotNewDirectory()
{
    QModelIndex parentIdx = ui->treeView->currentIndex();

    NodeDialog dlg(this);
    NodeData nd;
    nd.isDirectory = true;
    dlg.setNode(nd);
    dlg.setWindowTitle(tr("Новый каталог"));
    if (dlg.exec() != QDialog::Accepted) return;

    nd = dlg.node();
    nd.isDirectory = true;
    nd.ownerId     = m_currentUser.id;
    nd.parentId    = parentIdx.isValid() ? m_model->nodeAt(parentIdx).id : -1;

    if (!m_model->addNode(nd, parentIdx)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось создать каталог:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    ui->treeView->expand(parentIdx);
    statusBar()->showMessage(tr("Каталог '%1' создан").arg(nd.name), 3000);
}

void MainWindow::slotEditNode()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) return;

    NodeData nd = m_model->nodeAt(idx);
    NodeDialog dlg(this);
    dlg.setNode(nd);
    dlg.setWindowTitle(tr("Свойства узла"));
    if (dlg.exec() != QDialog::Accepted) return;

    NodeData updated = dlg.node();
    updated.id       = nd.id;
    updated.ownerId  = nd.ownerId;
    updated.parentId = nd.parentId;

    if (!m_model->updateNodeData(updated)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось обновить узел:\n%1")
                .arg(DatabaseManager::instance().lastError()));
    }
}

void MainWindow::slotDeleteNode()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) return;

    NodeData nd = m_model->nodeAt(idx);
    int ret = QMessageBox::question(this, tr("Удаление"),
        tr("Удалить узел «%1»?\nВсе дочерние узлы также будут удалены.").arg(nd.name),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // Close tabs referencing this node or its children
    for (int i = ui->tabWidget->count() - 1; i >= 0; --i) {
        if (m_tabNodeIds.value(i, -1) == nd.id)
            ui->tabWidget->removeTab(i);
    }

    if (!m_model->removeNode(idx)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось удалить узел:\n%1")
                .arg(DatabaseManager::instance().lastError()));
    }
}

void MainWindow::slotMoveNodeUp()
{
    // Simplified: just reload after drag-drop; up/down not trivially supported with QAbstractItemModel
    statusBar()->showMessage(tr("Используйте перетаскивание для перемещения узлов"), 3000);
}

void MainWindow::slotMoveNodeDown()
{
    statusBar()->showMessage(tr("Используйте перетаскивание для перемещения узлов"), 3000);
}

void MainWindow::slotAccessRights()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) return;

    NodeData nd = m_model->nodeAt(idx);
    AccessRightsDialog dlg(nd.id, m_currentUser.isAdmin, this);
    dlg.setWindowTitle(tr("Права доступа — %1").arg(nd.name));
    dlg.exec();
}

void MainWindow::slotReloadTree()
{
    m_model->reload();
    ui->treeView->expandAll();
    statusBar()->showMessage(tr("Дерево обновлено"), 2000);
}

// ---------------------------------------------------------------------------
// Content / tabs
// ---------------------------------------------------------------------------

void MainWindow::slotOpenNodeInTab(const QModelIndex& index)
{
    if (!index.isValid()) return;
    NodeData nd = m_model->nodeAt(index);
    if (nd.isDirectory) return;

    auto& db = DatabaseManager::instance();
    if (!db.hasRight(nd.id, m_currentUser.id, m_currentUser.isAdmin, "can_read")) {
        QMessageBox::warning(this, tr("Доступ запрещён"),
            tr("У вас нет права на чтение этого узла."));
        return;
    }

    // Check if already open
    int existingTab = findTabForNode(nd.id);
    if (existingTab >= 0) {
        ui->tabWidget->setCurrentIndex(existingTab);
        return;
    }

    openTab(nd);
}

void MainWindow::openTab(const NodeData& node)
{
    auto& db = DatabaseManager::instance();
    bool canWrite = db.hasRight(node.id, m_currentUser.id, m_currentUser.isAdmin, "can_write")
                 || node.ownerId == m_currentUser.id
                 || m_currentUser.isAdmin;

    // --- Container with vertical splitter: content editor on top, node tools below ---
    QWidget*     container    = new QWidget(this);
    QVBoxLayout* outerLayout  = new QVBoxLayout(container);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QSplitter* vSplitter = new QSplitter(Qt::Vertical, container);

    // Content editor
    QTextEdit* contentEdit = new QTextEdit(vSplitter);
    contentEdit->setObjectName("contentEditor");
    contentEdit->setPlainText(db.getNodeContent(node.id));
    contentEdit->setReadOnly(!canWrite);

    QTabWidget* bottomTabs = new QTabWidget(vSplitter);
    bottomTabs->setDocumentMode(true);

    QWidget* rolePage = new QWidget(bottomTabs);
    QVBoxLayout* roleLayout = new QVBoxLayout(rolePage);
    roleLayout->setContentsMargins(6, 6, 6, 6);
    roleLayout->setSpacing(6);

    QLabel* roleHint = new QLabel(tr("Роль применяется к абзацу, в котором стоит курсор."), rolePage);
    roleHint->setWordWrap(true);
    roleLayout->addWidget(roleHint);

    QHBoxLayout* roleButtonsLayout = new QHBoxLayout();
    roleButtonsLayout->setSpacing(4);
    roleLayout->addLayout(roleButtonsLayout);

    QHBoxLayout* roleSummaryLayout = new QHBoxLayout();
    roleSummaryLayout->setSpacing(8);
    roleLayout->addLayout(roleSummaryLayout);

    QLabel* actionsTitle = new QLabel(tr("Следующие шаги:"), rolePage);
    roleLayout->addWidget(actionsTitle);

    QListWidget* actionList = new QListWidget(rolePage);
    actionList->setObjectName("lineRoleActionList");
    actionList->setMaximumHeight(90);
    roleLayout->addWidget(actionList);

    const QVector<QPair<QString, QString>> lineRoles = {
        { "?",  tr("Вопрос") },
        { "!",  tr("Важное") },
        { "OK", tr("Решение") },
        { "->", tr("Следующий шаг") },
        { "#",  tr("Тема") },
        { "@",  tr("Участник") },
        { "...", tr("Ожидание") }
    };

    QMap<QString, QLabel*> roleCounters;
    for (const auto& role : lineRoles) {
        QLabel* counter = new QLabel(rolePage);
        counter->setMinimumWidth(72);
        roleSummaryLayout->addWidget(counter);
        roleCounters[role.first] = counter;
    }
    roleSummaryLayout->addStretch();

    auto rolePrefix = [](const QString& marker) {
        return QString("[%1] ").arg(marker);
    };

    auto firstTextIndex = [](const QString& line) {
        int pos = 0;
        while (pos < line.size() && line.at(pos).isSpace())
            ++pos;
        return pos;
    };

    auto roleForLine = [lineRoles, rolePrefix, firstTextIndex](const QString& line) {
        const QString text = line.mid(firstTextIndex(line));
        for (const auto& role : lineRoles) {
            if (text.startsWith(rolePrefix(role.first)))
                return role.first;
        }
        return QString();
    };

    auto withoutRole = [lineRoles, rolePrefix, firstTextIndex](const QString& line) {
        const int textPos = firstTextIndex(line);
        const QString indent = line.left(textPos);
        QString text = line.mid(textPos);
        for (const auto& role : lineRoles) {
            const QString prefix = rolePrefix(role.first);
            if (text.startsWith(prefix))
                return indent + text.mid(prefix.size());
        }
        return line;
    };

    auto updateRolePanel = [contentEdit, actionList, roleCounters, lineRoles, roleForLine]() {
        QMap<QString, int> counts;
        for (const auto& role : lineRoles)
            counts[role.first] = 0;

        actionList->clear();
        const QStringList lines = contentEdit->toPlainText().split('\n');
        for (const QString& line : lines) {
            const QString marker = roleForLine(line);
            if (marker.isEmpty())
                continue;

            counts[marker]++;
            if (marker == "->") {
                QString action = line.trimmed();
                action.remove(0, QString("[->] ").size());
                if (!action.trimmed().isEmpty())
                    actionList->addItem(action.trimmed());
            }
        }

        for (const auto& role : lineRoles) {
            QLabel* counter = roleCounters.value(role.first, nullptr);
            if (counter)
                counter->setText(QString("[%1] %2").arg(role.first).arg(counts.value(role.first)));
        }

        if (actionList->count() == 0)
            actionList->addItem(QObject::tr("Нет строк со следующими шагами"));
    };

    auto applyLineRole = [contentEdit, canWrite, rolePrefix, firstTextIndex, withoutRole, updateRolePanel](const QString& marker) {
        if (!canWrite)
            return;

        QTextCursor cursor = contentEdit->textCursor();
        cursor.select(QTextCursor::BlockUnderCursor);
        const QString cleaned = withoutRole(cursor.selectedText());
        const int textPos = firstTextIndex(cleaned);
        const QString replacement = cleaned.left(textPos) + rolePrefix(marker) + cleaned.mid(textPos).trimmed();
        cursor.insertText(replacement);
        contentEdit->setTextCursor(cursor);
        updateRolePanel();
    };

    for (const auto& role : lineRoles) {
        QPushButton* button = new QPushButton(QString("[%1]").arg(role.first), rolePage);
        button->setToolTip(role.second);
        button->setEnabled(canWrite);
        button->setMinimumWidth(42);
        roleButtonsLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [applyLineRole, marker = role.first]{
            applyLineRole(marker);
        });
    }
    roleButtonsLayout->addStretch();

    // Attributes group
    QGroupBox*      attrGroup  = new QGroupBox(tr("Атрибуты узла"), bottomTabs);
    QVBoxLayout*    attrLayout = new QVBoxLayout(attrGroup);
    attrLayout->setContentsMargins(4, 4, 4, 4);
    QPlainTextEdit* attrEdit   = new QPlainTextEdit(attrGroup);
    attrEdit->setObjectName("attrEditor");
    attrEdit->setPlainText(node.attributes);
    attrEdit->setReadOnly(!canWrite);
    attrEdit->setPlaceholderText(tr("Произвольные атрибуты (ключ: значение)"));
    attrLayout->addWidget(attrEdit);

    bottomTabs->addTab(rolePage, tr("Роли строк"));
    bottomTabs->addTab(attrGroup, tr("Атрибуты"));

    vSplitter->addWidget(contentEdit);
    vSplitter->addWidget(bottomTabs);
    vSplitter->setSizes({500, 170});
    outerLayout->addWidget(vSplitter);

    if (canWrite) {
        connect(contentEdit, &QTextEdit::textChanged,
                this, &MainWindow::slotContentChanged);
        connect(attrEdit, &QPlainTextEdit::textChanged,
                this, &MainWindow::slotContentChanged);
    }
    connect(contentEdit, &QTextEdit::textChanged, this, updateRolePanel);
    updateRolePanel();

    int tabIdx = ui->tabWidget->addTab(container, node.name);
    ui->tabWidget->setCurrentIndex(tabIdx);
    m_tabNodeIds[tabIdx] = node.id;
    m_tabDirty[tabIdx]   = false;

    ui->tabWidget->setVisible(true);
    ui->labelHint->setVisible(false);
}

int MainWindow::findTabForNode(int nodeId) const
{
    for (auto it = m_tabNodeIds.constBegin(); it != m_tabNodeIds.constEnd(); ++it) {
        if (it.value() == nodeId) return it.key();
    }
    return -1;
}

void MainWindow::slotTabCloseRequested(int index)
{
    if (m_tabDirty.value(index, false)) {
        int ret = QMessageBox::question(this, tr("Сохранение"),
            tr("Вкладка «%1» содержит несохранённые изменения. Сохранить?")
                .arg(ui->tabWidget->tabText(index)),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) {
            ui->tabWidget->setCurrentIndex(index);
            slotSaveContent();
        }
    }

    int nodeId = m_tabNodeIds.take(index);
    m_tabDirty.remove(index);
    (void)nodeId;

    // Remap keys after removal
    QMap<int,int>  newIds;
    QMap<int,bool> newDirty;
    for (auto it = m_tabNodeIds.begin(); it != m_tabNodeIds.end(); ++it) {
        int newKey = it.key() > index ? it.key() - 1 : it.key();
        newIds[newKey] = it.value();
    }
    for (auto it = m_tabDirty.begin(); it != m_tabDirty.end(); ++it) {
        int newKey = it.key() > index ? it.key() - 1 : it.key();
        newDirty[newKey] = it.value();
    }
    m_tabNodeIds = newIds;
    m_tabDirty   = newDirty;

    ui->tabWidget->removeTab(index);

    if (ui->tabWidget->count() == 0) {
        ui->tabWidget->setVisible(false);
        ui->labelHint->setVisible(true);
    }
}

void MainWindow::slotContentChanged()
{
    int idx = ui->tabWidget->currentIndex();
    if (idx < 0) return;
    m_tabDirty[idx] = true;
    QString title = ui->tabWidget->tabText(idx);
    if (!title.endsWith(" *"))
        ui->tabWidget->setTabText(idx, title + " *");
}

void MainWindow::slotSaveContent()
{
    int idx = ui->tabWidget->currentIndex();
    if (idx < 0) return;

    QWidget* container = ui->tabWidget->currentWidget();
    if (!container) return;

    auto* contentEdit = container->findChild<QTextEdit*>("contentEditor");
    auto* attrEdit    = container->findChild<QPlainTextEdit*>("attrEditor");
    if (!contentEdit) return;

    int nodeId = m_tabNodeIds.value(idx, -1);
    if (nodeId < 0) return;

    auto& db = DatabaseManager::instance();

    // Save content to node_content table
    if (!db.setNodeContent(nodeId, contentEdit->toPlainText())) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось сохранить содержимое:\n%1").arg(db.lastError()));
        return;
    }

    // Save attributes back to nodes table
    if (attrEdit) {
        NodeData nd = db.getNode(nodeId);
        nd.attributes = attrEdit->toPlainText();
        db.updateNode(nd);
    }

    m_tabDirty[idx] = false;
    QString title = ui->tabWidget->tabText(idx);
    if (title.endsWith(" *"))
        ui->tabWidget->setTabText(idx, title.chopped(2));
    statusBar()->showMessage(tr("Сохранено"), 2000);
}

void MainWindow::slotLoadFromFile()
{
    int idx = ui->tabWidget->currentIndex();
    if (idx < 0) {
        QMessageBox::information(this, tr("Загрузка"),
            tr("Сначала откройте узел во вкладке."));
        return;
    }
    auto* contentEdit = ui->tabWidget->currentWidget()
                            ? ui->tabWidget->currentWidget()->findChild<QTextEdit*>("contentEditor")
                            : nullptr;
    if (!contentEdit || contentEdit->isReadOnly()) {
        QMessageBox::warning(this, tr("Доступ"), tr("Редактирование этого узла недоступно."));
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Загрузить из файла"), {},
        tr("Текстовые файлы (*.txt *.md *.html);;Все файлы (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось открыть файл:\n%1").arg(path));
        return;
    }
    contentEdit->setPlainText(QTextStream(&f).readAll());
    slotContentChanged();
}

void MainWindow::slotSaveToFile()
{
    int idx = ui->tabWidget->currentIndex();
    if (idx < 0) return;
    auto* contentEdit = ui->tabWidget->currentWidget()
                            ? ui->tabWidget->currentWidget()->findChild<QTextEdit*>("contentEditor")
                            : nullptr;
    if (!contentEdit) return;

    QString path = QFileDialog::getSaveFileName(this, tr("Сохранить в файл"), {},
        tr("Текстовые файлы (*.txt);;Все файлы (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось сохранить файл:\n%1").arg(path));
        return;
    }
    QTextStream(&f) << contentEdit->toPlainText();
    statusBar()->showMessage(tr("Файл сохранён: %1").arg(path), 3000);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void MainWindow::slotSearch()
{
    SearchDialog dlg(m_currentUser.id, m_currentUser.isAdmin, this);
    connect(&dlg, &SearchDialog::nodeSelected, this, [this](int nodeId){
        QModelIndex idx = m_model->indexForNodeId(nodeId);
        if (idx.isValid()) {
            ui->treeView->setCurrentIndex(idx);
            ui->treeView->scrollTo(idx);
            slotOpenNodeInTab(idx);
        }
    });
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Admin
// ---------------------------------------------------------------------------

void MainWindow::slotAdminPanel()
{
    if (!m_currentUser.isAdmin) return;
    AdminDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void MainWindow::updateStatusBar()
{
    m_statusLabel->setText(tr("Пользователь: %1%2")
        .arg(m_currentUser.username,
             m_currentUser.isAdmin ? tr(" [Администратор]") : QString()));
}
