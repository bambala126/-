#include "mainwindow.h"
#include "logindialog.h"
#include "databasemanager.h"

#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("TreeNote");
    a.setApplicationDisplayName(QObject::tr("Древовидный блокнот"));
    a.setOrganizationName("TreeNote");

    // Prepare app data directory
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    QString dbPath = dataPath + "/treenote.db";

    // Open SQLite database
    if (!DatabaseManager::instance().openDatabase(dbPath)) {
        QMessageBox::critical(nullptr, QObject::tr("Ошибка"),
            QObject::tr("Не удалось открыть базу данных:\n%1\n\n%2")
                .arg(dbPath, DatabaseManager::instance().lastError()));
        return 1;
    }

    // Login
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted)
        return 0;

    const UserData& user = loginDlg.currentUser();

    // Register current user in DB layer so all write operations validate permissions internally
    DatabaseManager::instance().setCurrentUser(user.id, user.isAdmin);

    MainWindow w(user);
    w.show();

    return a.exec();
}
