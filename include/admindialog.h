#pragma once

#include <QDialog>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AdminDialog; }
QT_END_NAMESPACE

class AdminDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdminDialog(QWidget* parent = nullptr);
    ~AdminDialog() override;

private slots:
    // Users tab
    void slotAddUser();
    void slotEditUser();
    void slotDeleteUser();
    void slotUserSelectionChanged();
    void slotChangePassword();

    // Groups tab
    void slotAddGroup();
    void slotEditGroup();
    void slotDeleteGroup();
    void slotGroupSelectionChanged();

    void refreshUsers();
    void refreshGroups();

private:
    Ui::AdminDialog* ui;
};
