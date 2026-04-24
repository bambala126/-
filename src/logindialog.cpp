#include "logindialog.h"
#include "ui_logindialog.h"
#include "databasemanager.h"

#include <QMessageBox>

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Вход в систему"));
    setFixedSize(sizeHint());

    connect(ui->pushButtonLogin, &QPushButton::clicked,
            this, &LoginDialog::slotLogin);
    connect(ui->lineEditPassword, &QLineEdit::returnPressed,
            this, &LoginDialog::slotLogin);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::slotLogin()
{
    QString username = ui->lineEditUsername->text().trimmed();
    QString password = ui->lineEditPassword->text();

    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Введите имя пользователя."));
        ui->lineEditUsername->setFocus();
        return;
    }

    if (!DatabaseManager::instance().authenticateUser(username, password, m_user)) {
        QMessageBox::warning(this, tr("Ошибка входа"),
            tr("Неверное имя пользователя или пароль."));
        ui->lineEditPassword->clear();
        ui->lineEditPassword->setFocus();
        return;
    }

    accept();
}
