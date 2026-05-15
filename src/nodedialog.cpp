#include "nodedialog.h"
#include "ui_nodedialog.h"

NodeDialog::NodeDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::NodeDialog)
{
    ui->setupUi(this);
    ui->plainTextEditAttributes->setPlaceholderText(
        tr("Свободные атрибуты в формате \"ключ: значение\"\n"
           "\n"
           "Например:\n"
           "тип: документ\n"
           "статус: черновик\n"
           "теги: учеба, заметки"));
    setWindowTitle(tr("Узел"));
}

NodeDialog::~NodeDialog()
{
    delete ui;
}

void NodeDialog::setNode(const NodeData& node)
{
    ui->lineEditName->setText(node.name);
    ui->checkBoxDirectory->setChecked(node.isDirectory);
    ui->plainTextEditAttributes->setPlainText(node.attributes);
}

NodeData NodeDialog::node() const
{
    NodeData nd;
    nd.name        = ui->lineEditName->text().trimmed();
    nd.isDirectory = ui->checkBoxDirectory->isChecked();
    nd.attributes  = ui->plainTextEditAttributes->toPlainText();
    return nd;
}
