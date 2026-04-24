#include "accessrightsdialog.h"
#include "ui_accessrightsdialog.h"
#include "databasemanager.h"

#include <QMessageBox>
#include <QTableWidgetItem>

AccessRightsDialog::AccessRightsDialog(int nodeId, bool isAdmin, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AccessRightsDialog)
    , m_nodeId(nodeId)
    , m_isAdmin(isAdmin)
{
    ui->setupUi(this);
    setWindowTitle(tr("Права доступа"));
    resize(700, 420);

    // Table setup
    ui->tableWidget->setColumnCount(6);
    ui->tableWidget->setHorizontalHeaderLabels(
        {tr("Субъект"), tr("Тип"), tr("Видеть"), tr("Читать"), tr("Изменять"), tr("Создавать потомков")});
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

    // Type combo
    ui->comboBoxType->addItems({tr("Пользователь"), tr("Группа")});

    connect(ui->pushButtonAdd,    &QPushButton::clicked,
            this, &AccessRightsDialog::slotAddRight);
    connect(ui->pushButtonRemove, &QPushButton::clicked,
            this, &AccessRightsDialog::slotRemoveRight);
    connect(ui->tableWidget,      &QTableWidget::itemSelectionChanged,
            this, &AccessRightsDialog::slotRightSelectionChanged);
    connect(ui->comboBoxType,     QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AccessRightsDialog::slotTypeChanged);

    slotTypeChanged(0);
    refreshTable();
    slotRightSelectionChanged();
}

AccessRightsDialog::~AccessRightsDialog()
{
    delete ui;
}

void AccessRightsDialog::refreshTable()
{
    auto rights = DatabaseManager::instance().getAccessRights(m_nodeId);
    auto users  = DatabaseManager::instance().getUsers();
    auto groups = DatabaseManager::instance().getGroups();

    QMap<int,QString> userNames, groupNames;
    for (const UserData& u : users)   userNames[u.id]  = u.username;
    for (const GroupData& g : groups) groupNames[g.id] = g.name;

    ui->tableWidget->setRowCount(rights.size());
    for (int r = 0; r < rights.size(); ++r) {
        const AccessRightData& ar = rights[r];
        QString subject, type;
        if (ar.userId > 0) {
            subject = userNames.value(ar.userId, QString::number(ar.userId));
            type    = tr("Пользователь");
        } else {
            subject = groupNames.value(ar.groupId, QString::number(ar.groupId));
            type    = tr("Группа");
        }

        ui->tableWidget->setItem(r, 0, new QTableWidgetItem(subject));
        ui->tableWidget->setItem(r, 1, new QTableWidgetItem(type));
        ui->tableWidget->setItem(r, 2, new QTableWidgetItem(ar.canSee            ? tr("Да") : tr("Нет")));
        ui->tableWidget->setItem(r, 3, new QTableWidgetItem(ar.canRead           ? tr("Да") : tr("Нет")));
        ui->tableWidget->setItem(r, 4, new QTableWidgetItem(ar.canWrite          ? tr("Да") : tr("Нет")));
        ui->tableWidget->setItem(r, 5, new QTableWidgetItem(ar.canCreateChildren ? tr("Да") : tr("Нет")));

        // Store right id for removal
        ui->tableWidget->item(r, 0)->setData(Qt::UserRole, ar.id);
    }
    ui->tableWidget->resizeColumnsToContents();
}

void AccessRightsDialog::slotAddRight()
{
    int typeIdx = ui->comboBoxType->currentIndex(); // 0=user, 1=group
    int subjectId = ui->comboBoxSubject->currentData().toInt();
    if (subjectId <= 0) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Выберите субъект."));
        return;
    }

    AccessRightData ar;
    ar.nodeId           = m_nodeId;
    ar.canSee           = ui->checkBoxSee->isChecked();
    ar.canRead          = ui->checkBoxRead->isChecked();
    ar.canWrite         = ui->checkBoxWrite->isChecked();
    ar.canCreateChildren= ui->checkBoxCreate->isChecked();

    if (typeIdx == 0) ar.userId  = subjectId;
    else              ar.groupId = subjectId;

    bool ok = ui->checkBoxApplyToSubtree->isChecked()
        ? DatabaseManager::instance().setAccessRightRecursive(ar)
        : DatabaseManager::instance().setAccessRight(ar);
    if (!ok) {
        QMessageBox::warning(this, tr("Ошибка"),
            tr("Не удалось добавить право:\n%1")
                .arg(DatabaseManager::instance().lastError()));
        return;
    }
    refreshTable();
}

void AccessRightsDialog::slotRemoveRight()
{
    int row = ui->tableWidget->currentRow();
    if (row < 0) return;
    int rightId = ui->tableWidget->item(row, 0)->data(Qt::UserRole).toInt();

    if (QMessageBox::question(this, tr("Удаление"),
            tr("Удалить выбранное право доступа?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    DatabaseManager::instance().deleteAccessRight(rightId);
    refreshTable();
}

void AccessRightsDialog::slotRightSelectionChanged()
{
    ui->pushButtonRemove->setEnabled(ui->tableWidget->currentRow() >= 0);
}

void AccessRightsDialog::slotTypeChanged(int index)
{
    populateSubjectCombo(index);
}

void AccessRightsDialog::populateSubjectCombo(int typeIndex)
{
    ui->comboBoxSubject->clear();
    if (typeIndex == 0) {
        auto users = DatabaseManager::instance().getUsers();
        for (const UserData& u : users)
            ui->comboBoxSubject->addItem(u.username, u.id);
    } else {
        auto groups = DatabaseManager::instance().getGroups();
        for (const GroupData& g : groups)
            ui->comboBoxSubject->addItem(g.name, g.id);
    }
}
