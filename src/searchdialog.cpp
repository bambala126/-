#include "searchdialog.h"
#include "ui_searchdialog.h"
#include "databasemanager.h"

#include <QListWidgetItem>

SearchDialog::SearchDialog(int userId, bool isAdmin, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SearchDialog)
    , m_userId(userId)
    , m_isAdmin(isAdmin)
{
    ui->setupUi(this);
    setWindowTitle(tr("Поиск"));
    resize(520, 420);

    connect(ui->pushButtonSearch, &QPushButton::clicked,
            this, &SearchDialog::slotSearch);
    connect(ui->lineEditQuery, &QLineEdit::returnPressed,
            this, &SearchDialog::slotSearch);
    connect(ui->listWidgetResults, &QListWidget::itemDoubleClicked,
            this, &SearchDialog::slotResultDoubleClicked);
}

SearchDialog::~SearchDialog()
{
    delete ui;
}

void SearchDialog::slotSearch()
{
    QString query = ui->lineEditQuery->text().trimmed();
    if (query.isEmpty()) return;

    ui->listWidgetResults->clear();
    m_selectedNodeId = -1;

    auto& db = DatabaseManager::instance();
    QList<NodeData> results;

    if (ui->checkBoxName->isChecked())
        results += db.searchByName(query, m_userId, m_isAdmin);

    if (ui->checkBoxAttributes->isChecked()) {
        auto ar = db.searchByAttributes(query, m_userId, m_isAdmin);
        for (const NodeData& n : ar)
            if (!results.contains(n)) results << n;
    }

    if (ui->checkBoxContent->isChecked()) {
        auto cr = db.searchByContent(query, m_userId, m_isAdmin);
        for (const NodeData& n : cr)
            if (!results.contains(n)) results << n;
    }

    for (const NodeData& n : results) {
        QString label = n.isDirectory
            ? tr("[Каталог] %1").arg(n.name)
            : n.name;
        auto* item = new QListWidgetItem(label, ui->listWidgetResults);
        item->setData(Qt::UserRole, n.id);
        item->setToolTip(n.attributes);
    }

    ui->labelResultCount->setText(tr("Найдено: %1").arg(results.size()));
}

void SearchDialog::slotResultDoubleClicked()
{
    auto* item = ui->listWidgetResults->currentItem();
    if (!item) return;
    m_selectedNodeId = item->data(Qt::UserRole).toInt();
    emit nodeSelected(m_selectedNodeId);
    accept();
}
