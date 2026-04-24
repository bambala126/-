#pragma once

#include <QDialog>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AccessRightsDialog; }
QT_END_NAMESPACE

class AccessRightsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AccessRightsDialog(int nodeId, bool isAdmin, QWidget* parent = nullptr);
    ~AccessRightsDialog() override;

private slots:
    void slotAddRight();
    void slotRemoveRight();
    void slotRightSelectionChanged();
    void slotTypeChanged(int index);   // user vs group
    void refreshTable();

private:
    void populateSubjectCombo(int typeIndex);

    Ui::AccessRightsDialog* ui;
    int  m_nodeId;
    bool m_isAdmin;
};
