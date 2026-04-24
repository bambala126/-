#pragma once

#include <QDialog>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class NodeDialog; }
QT_END_NAMESPACE

class NodeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NodeDialog(QWidget* parent = nullptr);
    ~NodeDialog() override;

    void    setNode(const NodeData& node);
    NodeData node() const;

private:
    Ui::NodeDialog* ui;
};
