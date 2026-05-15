#include "admindialog.h"
#include "ui_admindialog.h"
#include "databasemanager.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QTableWidgetItem>

AdminDialog::AdminDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AdminDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Администрирование"));
    resize(640, 480);

    // Users tab
    ui->tableWidgetUsers->setColumnCount(4);
    ui->tableWidgetUsers->setHorizontalHeaderLabels(
        {tr("ID"), tr("Имя пользователя"), tr("Группа"), tr("Администратор")});
    ui->tableWidgetUsers->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidgetUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Groups tab
    ui->tableWidgetGroups->setColumnCount(2);
    ui->tableWidgetGroups->setHorizontalHeaderLabels({tr("ID"), tr("Название")});
    ui->tableWidgetGroups->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidgetGroups->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetGroups->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->pushButtonAddUser,    &QPushButton::clicked, this, &AdminDialog::slotAddUser);
    connect(ui->pushButtonEditUser,   &QPushButton::clicked, this, &AdminDialog::slotEditUser);
    connect(ui->pushButtonDeleteUser, &QPushButton::clicked, this, &AdminDialog::slotDeleteUser);
    connect(ui->pushButtonChangePass, &QPushButton::clicked, this, &AdminDialog::slotChangePassword);
    connect(ui->tableWidgetUsers,     &QTableWidget::itemSelectionChanged,
            this, &AdminDialog::slotUserSelectionChanged);

    connect(ui->pushButtonAddGroup,    &QPushButton::clicked, this, &AdminDialog::slotAddGroup);
    connect(ui->pushButtonEditGroup,   &QPushButton::clicked, this, &AdminDialog::slotEditGroup);
    connect(ui->pushButtonDeleteGroup, &QPushButton::clicked, this, &AdminDialog::slotDeleteGroup);
    connect(ui->tableWidgetGroups,     &QTableWidget::itemSelectionChanged,
            this, &AdminDialog::slotGroupSelectionChanged);

    refreshUsers();
    refreshGroups();
    slotUserSelectionChanged();
    slotGroupSelectionChanged();
}

AdminDialog::~AdminDialog()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

void AdminDialog::refreshUsers()
{
    auto users  = DatabaseManager::instance().getUsers();
    auto groups = DatabaseManager::instance().getGroups();

    QMap<int,QString> groupNames;
    for (const GroupData& g : groups) groupNames[g.id] = g.name;

    ui->tableWidgetUsers->setRowCount(users.size());
    for (int r = 0; r < users.size(); ++r) {
        const UserData& u = users[r];
        ui->tableWidgetUsers->setItem(r, 0, new QTableWidgetItem(QString::number(u.id)));
        ui->tableWidgetUsers->setItem(r, 1, new QTableWidgetItem(u.username));
        ui->tableWidgetUsers->setItem(r, 2, new QTableWidgetItem(
            u.groupId > 0 ? groupNames.value(u.groupId, tr("—")) : tr("—")));
        ui->tableWidgetUsers->setItem(r, 3, new QTableWidgetItem(
            u.isAdmin ? tr("Да") : tr("Нет")));
        ui->tableWidgetUsers->item(r, 0)->setData(Qt::UserRole, u.id);
    }
    ui->tableWidgetUsers->resizeColumnsToContents();
}

void AdminDialog::slotAddUser()
{
    bool ok;
    QString username = QInputDialog::getText(this, tr("Новый пользователь"),
        tr("Имя пользователя:"), QLineEdit::Normal, {}, &ok);
    if (!ok || username.trimmed().isEmpty()) return;

    QString password = QInputDialog::getText(this, tr("Новый пользователь"),
        tr("Пароль:"), QLineEdit::Password, {}, &ok);
    if (!ok) return;

    QStringList groupItems = {tr("— нет группы —")};
    auto groups = DatabaseManager::instance().getGroups();
    for (const GroupData& g : groups) groupItems << g.name;

    QString groupChoice = QInputDialog::getItem(this, tr("Группа"),
        tr("Выберите группу:"), groupItems, 0, false, &ok);
    if (!ok) return;

    int groupId = -1;
    if (groupChoice != groupItems.first()) {
        for (const GroupData& g : groups)
            if (g.name == groupChoice) { groupId = g.id; break; }
    }

    bool isAdmin = QMessageBox::question(this, tr("Права"),
        tr("Сделать пользователя администратором?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;

    UserData u;
    u.username = username.trimmed();
    u.isAdmin  = isAdmin;
    u.groupId  = groupId;

    if (!DatabaseManager::instance().createUser(u, password)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось создать пользователя:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    refreshUsers();
}

void AdminDialog::slotEditUser()
{
    int row = ui->tableWidgetUsers->currentRow();
    if (row < 0) return;
    int userId = ui->tableWidgetUsers->item(row, 0)->data(Qt::UserRole).toInt();

    UserData u = DatabaseManager::instance().getUser(userId);

    bool ok;
    QString username = QInputDialog::getText(this, tr("Редактирование"),
        tr("Имя пользователя:"), QLineEdit::Normal, u.username, &ok);
    if (!ok || username.trimmed().isEmpty()) return;
    u.username = username.trimmed();

    auto groups = DatabaseManager::instance().getGroups();
    QStringList groupItems = {tr("— нет группы —")};
    int currentGroupIdx = 0;
    for (int i = 0; i < groups.size(); ++i) {
        groupItems << groups[i].name;
        if (groups[i].id == u.groupId) currentGroupIdx = i + 1;
    }
    QString groupChoice = QInputDialog::getItem(this, tr("Группа"),
        tr("Выберите группу:"), groupItems, currentGroupIdx, false, &ok);
    if (!ok) return;

    u.groupId = -1;
    if (groupChoice != groupItems.first()) {
        for (const GroupData& g : groups)
            if (g.name == groupChoice) { u.groupId = g.id; break; }
    }

    u.isAdmin = QMessageBox::question(this, tr("Права"),
        tr("Сделать пользователя администратором?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;

    if (!DatabaseManager::instance().updateUser(u)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось обновить пользователя:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    refreshUsers();
}

void AdminDialog::slotDeleteUser()
{
    int row = ui->tableWidgetUsers->currentRow();
    if (row < 0) return;
    int userId = ui->tableWidgetUsers->item(row, 0)->data(Qt::UserRole).toInt();
    QString name = ui->tableWidgetUsers->item(row, 1)->text();

    if (QMessageBox::question(this, tr("Удаление"),
            tr("Удалить пользователя «%1»?").arg(name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    DatabaseManager::instance().deleteUser(userId);
    refreshUsers();
}

void AdminDialog::slotChangePassword()
{
    int row = ui->tableWidgetUsers->currentRow();
    if (row < 0) return;
    int userId = ui->tableWidgetUsers->item(row, 0)->data(Qt::UserRole).toInt();

    bool ok;
    QString newPass = QInputDialog::getText(this, tr("Смена пароля"),
        tr("Новый пароль:"), QLineEdit::Password, {}, &ok);
    if (!ok || newPass.isEmpty()) return;

    if (!DatabaseManager::instance().updateUserPassword(userId, newPass)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось изменить пароль:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    QMessageBox::information(this, tr("Готово"), tr("Пароль успешно изменён."));
}

void AdminDialog::slotUserSelectionChanged()
{
    bool sel = ui->tableWidgetUsers->currentRow() >= 0;
    ui->pushButtonEditUser->setEnabled(sel);
    ui->pushButtonDeleteUser->setEnabled(sel);
    ui->pushButtonChangePass->setEnabled(sel);
}

// ---------------------------------------------------------------------------
// Groups
// ---------------------------------------------------------------------------

void AdminDialog::refreshGroups()
{
    auto groups = DatabaseManager::instance().getGroups();
    ui->tableWidgetGroups->setRowCount(groups.size());
    for (int r = 0; r < groups.size(); ++r) {
        ui->tableWidgetGroups->setItem(r, 0, new QTableWidgetItem(QString::number(groups[r].id)));
        ui->tableWidgetGroups->setItem(r, 1, new QTableWidgetItem(groups[r].name));
        ui->tableWidgetGroups->item(r, 0)->setData(Qt::UserRole, groups[r].id);
    }
    ui->tableWidgetGroups->resizeColumnsToContents();
}

void AdminDialog::slotAddGroup()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Новая группа"),
        tr("Название группы:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    GroupData g;
    g.name = name.trimmed();
    if (!DatabaseManager::instance().createGroup(g)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось создать группу:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    refreshGroups();
}

void AdminDialog::slotEditGroup()
{
    int row = ui->tableWidgetGroups->currentRow();
    if (row < 0) return;
    int groupId = ui->tableWidgetGroups->item(row, 0)->data(Qt::UserRole).toInt();
    QString oldName = ui->tableWidgetGroups->item(row, 1)->text();

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Редактирование группы"),
        tr("Название:"), QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    GroupData g;
    g.id   = groupId;
    g.name = newName.trimmed();
    if (!DatabaseManager::instance().updateGroup(g)) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось обновить группу:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    refreshGroups();
}

void AdminDialog::slotDeleteGroup()
{
    int row = ui->tableWidgetGroups->currentRow();
    if (row < 0) return;
    int groupId = ui->tableWidgetGroups->item(row, 0)->data(Qt::UserRole).toInt();
    QString name = ui->tableWidgetGroups->item(row, 1)->text();

    if (QMessageBox::question(this, tr("Удаление"),
            tr("Удалить группу «%1»?").arg(name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    DatabaseManager::instance().deleteGroup(groupId);
    refreshGroups();
}

void AdminDialog::slotGroupSelectionChanged()
{
    bool sel = ui->tableWidgetGroups->currentRow() >= 0;
    ui->pushButtonEditGroup->setEnabled(sel);
    ui->pushButtonDeleteGroup->setEnabled(sel);
}
