#pragma once

#include <QDialog>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog() override;

    const UserData& currentUser() const { return m_user; }

private slots:
    void slotLogin();

private:
    Ui::LoginDialog* ui;
    UserData         m_user;
};
