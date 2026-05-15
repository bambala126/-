#pragma once

#include <QDialog>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SearchDialog; }
QT_END_NAMESPACE

class SearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchDialog(int userId, bool isAdmin, QWidget* parent = nullptr);
    ~SearchDialog() override;

    int selectedNodeId() const { return m_selectedNodeId; }

signals:
    void nodeSelected(int nodeId);

private slots:
    void slotSearch();
    void slotResultDoubleClicked();

private:
    Ui::SearchDialog* ui;
    int  m_userId;
    bool m_isAdmin;
    int  m_selectedNodeId = -1;
};
