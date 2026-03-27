#include "UbicacionesPage.h"
#include "core/SupabaseClient.h"
#include "core/UbicacionService.h"
#include "ui/LoadingOverlay.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>

UbicacionesPage::UbicacionesPage(SupabaseClient* supabase, QWidget* parent)
    : QWidget(parent)
    , m_supabase(supabase)
    , m_service(new UbicacionService(supabase, this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(tr("Ubicaciones"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    layout->addWidget(title);

    auto* desc = new QLabel(tr("Gestiona las ubicaciones de las piezas"), this);
    desc->setStyleSheet(QStringLiteral("font-size: 13px; color: #7f8c8d;"));
    layout->addWidget(desc);
    layout->addSpacing(16);

    auto* btnLayout = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("A\u00f1adir"), this);
    m_editBtn = new QPushButton(tr("Editar"), this);
    m_deleteBtn = new QPushButton(tr("Eliminar"), this);
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    addBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:disabled { background-color: #95a5a6; }"));
    m_editBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e67e22; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #f39c12; }"
        "QPushButton:disabled { background-color: #95a5a6; }"));
    m_deleteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #c0392b; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e74c3c; }"
        "QPushButton:disabled { background-color: #95a5a6; }"));
    addBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch(1);
    layout->addLayout(btnLayout);
    layout->addSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({ tr("Id"), tr("Nombre") });
    m_table->setColumnHidden(0, true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->verticalHeader()->setVisible(false);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { gridline-color: #e8e8e8; outline: none; }"
        "QTableWidget::item { padding: 12px 16px; color: #1a1a1a; border: none; outline: none; }"
        "QTableWidget::item:hover { background-color: #f0f0f0; border: none; outline: none; }"
        "QTableWidget::item:selected { background-color: #c5d9e8; color: #1a1a1a; border: none; outline: none; }"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QHeaderView::section { padding: 12px 16px; background-color: #ecf0f1; color: #2c3e50; }"));
    m_table->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_table);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    layout->addWidget(m_countLabel, 0, Qt::AlignRight);

    m_overlay = new LoadingOverlay(this);

    connect(addBtn, &QPushButton::clicked, this, &UbicacionesPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &UbicacionesPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &UbicacionesPage::onDelete);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { onEdit(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        bool has = selectedId() > 0;
        m_editBtn->setEnabled(has);
        m_deleteBtn->setEnabled(has);
    });

    connect(m_service, &UbicacionService::dataReady, this, [this](const QVector<Ubicacion>& list) {
        m_overlay->hideOverlay();
        m_table->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            m_table->setItem(i, 0, new QTableWidgetItem(QString::number(list[i].id)));
            m_table->setItem(i, 1, new QTableWidgetItem(list[i].nombre));
        }
        m_countLabel->setText(tr("Mostrando %1 elementos").arg(list.size()));
    });
    connect(m_service, &UbicacionService::mutationDone, this, &UbicacionesPage::refreshTable);
    connect(m_service, &UbicacionService::errorOccurred, this, [this](const QString& msg) {
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Ubicaciones"), msg);
    });

    refreshTable();
}

void UbicacionesPage::refreshTable()
{
    m_overlay->showOverlay();
    m_service->fetchAll();
}

void UbicacionesPage::onAdd()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("A\u00f1adir ubicaci\u00f3n"));
    auto* form = new QFormLayout(&dlg);
    auto* nombreEdit = new QLineEdit(&dlg);
    nombreEdit->setPlaceholderText(tr("Nombre de la ubicaci\u00f3n"));
    nombreEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nombreEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString nombre = nombreEdit->text().trimmed();
    if (nombre.isEmpty()) {
        QMessageBox::warning(this, tr("Ubicaciones"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    m_overlay->showOverlay();
    m_service->create(nombre);
}

void UbicacionesPage::onEdit()
{
    int id = selectedId();
    if (id <= 0) return;
    int row = m_table->currentRow();
    QString currentNombre = m_table->item(row, 1)->text();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Editar ubicaci\u00f3n"));
    auto* form = new QFormLayout(&dlg);
    auto* nombreEdit = new QLineEdit(&dlg);
    nombreEdit->setText(currentNombre);
    nombreEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nombreEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString nombre = nombreEdit->text().trimmed();
    if (nombre.isEmpty()) {
        QMessageBox::warning(this, tr("Ubicaciones"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    m_overlay->showOverlay();
    m_service->update(id, nombre);
}

void UbicacionesPage::onDelete()
{
    int id = selectedId();
    if (id <= 0) return;
    QString nombre = m_table->item(m_table->currentRow(), 1)->text();
    auto ret = QMessageBox::question(this, tr("Eliminar ubicaci\u00f3n"),
        tr("\u00bfEliminar \"%1\"?").arg(nombre),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_overlay->showOverlay();
    m_service->remove(id);
}

int UbicacionesPage::selectedId() const
{
    int row = m_table->currentRow();
    if (row < 0) return 0;
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->text().toInt() : 0;
}
