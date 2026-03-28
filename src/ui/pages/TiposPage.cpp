#include "TiposPage.h"
#include "core/SupabaseClient.h"
#include "core/TipoService.h"
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
#include <QComboBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QTreeWidget>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>

static const QStringList VAR_TIPOS = { QStringLiteral("entero"), QStringLiteral("texto"), QStringLiteral("decimal"), QStringLiteral("booleano") };

TiposPage::TiposPage(SupabaseClient* supabase, HistorialService* historial, bool viewOnly,
                     QWidget* parent)
    : QWidget(parent)
    , m_viewOnly(viewOnly)
    , m_supabase(supabase)
    , m_service(new TipoService(supabase, historial, this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(tr("Tipos"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    layout->addWidget(title);

    auto* desc = new QLabel(tr("Gestiona los tipos y sus variables (params)"), this);
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
    if (m_viewOnly) {
        addBtn->setEnabled(false);
        m_editBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
    }

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch(1);
    layout->addLayout(btnLayout);
    layout->addSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({ tr("Id"), tr("Nombre"), tr("Acciones"), tr("Variables") });
    m_table->setColumnHidden(0, true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(1, 220);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
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
        "QTableWidget::item:selected { background-color: #e4e4e4; color: #1a1a1a; border: none; outline: none; }"
        "QTableWidget::item:selected:active { background-color: #dcdcdc; border: none; outline: none; }"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QHeaderView::section { padding: 12px 16px; background-color: #ecf0f1; color: #2c3e50; }"));
    m_table->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_table);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    layout->addWidget(m_countLabel, 0, Qt::AlignRight);

    m_overlay = new LoadingOverlay(this);

    connect(addBtn, &QPushButton::clicked, this, &TiposPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &TiposPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TiposPage::onDelete);
    if (!m_viewOnly)
        connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { onEdit(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        bool has = selectedId() > 0;
        const bool canEdit = !m_viewOnly && has;
        m_editBtn->setEnabled(canEdit);
        m_deleteBtn->setEnabled(canEdit);
    });

    connect(m_service, &TipoService::dataReady, this, [this](const QVector<Tipo>& list) {
        m_overlay->hideOverlay();
        m_table->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            auto* idItem = new QTableWidgetItem(QString::number(list[i].id));
            idItem->setData(Qt::UserRole, list[i].acciones);
            idItem->setData(Qt::UserRole + 1, list[i].params);
            m_table->setItem(i, 0, idItem);
            m_table->setItem(i, 1, new QTableWidgetItem(list[i].nombre));
            m_table->setItem(i, 2, new QTableWidgetItem(accionesToSummary(list[i].acciones)));
            m_table->setItem(i, 3, new QTableWidgetItem(paramsToSummary(list[i].params)));
        }
        m_countLabel->setText(tr("Mostrando %1 elementos").arg(list.size()));
    });
    connect(m_service, &TipoService::mutationDone, this, &TiposPage::refreshTable);
    connect(m_service, &TipoService::errorOccurred, this, [this](const QString& msg) {
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Tipos"), msg);
    });

    refreshTable();
}

QString TiposPage::paramsToSummary(const QString& paramsJson)
{
    if (paramsJson.isEmpty() || paramsJson == QLatin1String("{}")) return tr("—");
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return tr("—");
    QJsonObject obj = doc.object();
    if (obj.isEmpty()) return tr("—");
    QStringList names = obj.keys();
    return tr("%1 variable(s): %2").arg(names.size()).arg(names.join(QStringLiteral(", ")));
}

QString TiposPage::accionesToSummary(const QString& accionesJson)
{
    if (accionesJson.isEmpty() || accionesJson == QLatin1String("{}")) return tr("—");
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(accionesJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return tr("—");
    QJsonObject obj = doc.object();
    if (obj.isEmpty()) return tr("—");
    QStringList names = obj.keys();
    return tr("%1 acci\u00f3n(es): %2").arg(names.size()).arg(names.join(QStringLiteral(", ")));
}

QString TiposPage::buildAccionesJson(QTreeWidget* accTree)
{
    QJsonObject root;
    for (int i = 0; i < accTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* accItem = accTree->topLevelItem(i);
        QString accion = accItem->text(0).trimmed();
        if (accion.isEmpty()) continue;
        QJsonObject paramsObj;
        for (int j = 0; j < accItem->childCount(); ++j) {
            QTreeWidgetItem* paramItem = accItem->child(j);
            QString param = paramItem->text(0).trimmed();
            QWidget* w1 = accTree->itemWidget(paramItem, 1);
            QWidget* w2 = accTree->itemWidget(paramItem, 2);
            QWidget* w3 = accTree->itemWidget(paramItem, 3);
            auto* combo = qobject_cast<QComboBox*>(w1);
            auto* optCheck = qobject_cast<QCheckBox*>(w2);
            auto* descEdit = qobject_cast<QLineEdit*>(w3);
            if (!combo) continue;
            QJsonObject paramDef;
            paramDef.insert(QStringLiteral("tipo"), combo->currentText());
            paramDef.insert(QStringLiteral("opcional"), optCheck && optCheck->isChecked());
            paramDef.insert(QStringLiteral("descripcion"), descEdit ? descEdit->text().trimmed() : QString());
            if (!param.isEmpty())
                paramsObj.insert(param, paramDef);
        }
        root.insert(accion, paramsObj);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

static QJsonObject paramValueToObj(const QJsonValue& v)
{
    if (v.isObject()) return v.toObject();
    QJsonObject o;
    o.insert(QStringLiteral("tipo"), v.toString().isEmpty() ? QStringLiteral("texto") : v.toString());
    o.insert(QStringLiteral("opcional"), false);
    o.insert(QStringLiteral("descripcion"), QString());
    return o;
}

void TiposPage::fillAccionesTree(const QString& accionesJson, QTreeWidget* accTree)
{
    accTree->clear();
    if (accionesJson.isEmpty() || accionesJson == QLatin1String("{}")) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(accionesJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString accion = it.key();
        if (!it.value().isObject()) continue;
        QJsonObject params = it.value().toObject();
        auto* accItem = new QTreeWidgetItem(accTree, { accion });
        accItem->setFlags(accItem->flags() | Qt::ItemIsEditable);
        accItem->setExpanded(true);
        if (!params.isEmpty()) {
            for (auto pit = params.begin(); pit != params.end(); ++pit) {
                QString paramName = pit.key();
                if (paramName == QLatin1String("_")) paramName = QString();
                QJsonObject p = paramValueToObj(pit.value());
                QString tipo = p.value(QStringLiteral("tipo")).toString();
                if (tipo.isEmpty()) tipo = QStringLiteral("texto");
                bool opcional = p.value(QStringLiteral("opcional")).toBool();
                QString desc = p.value(QStringLiteral("descripcion")).toString();
                auto* child = new QTreeWidgetItem(accItem, { paramName });
                child->setFlags(child->flags() | Qt::ItemIsEditable);
                auto* combo = new QComboBox(accTree);
                combo->addItems(VAR_TIPOS);
                int idx = VAR_TIPOS.indexOf(tipo);
                if (idx >= 0) combo->setCurrentIndex(idx);
                auto* optCheck = new QCheckBox(accTree);
                optCheck->setChecked(opcional);
                auto* descEdit = new QLineEdit(accTree);
                descEdit->setText(desc);
                descEdit->setPlaceholderText(tr("Descripci\u00f3n"));
                accTree->setItemWidget(child, 1, combo);
                accTree->setItemWidget(child, 2, optCheck);
                accTree->setItemWidget(child, 3, descEdit);
            }
        }
    }
}

static const char* treeItemStyle = R"(
    QTreeWidget { outline: none; }
    QTreeWidget::item { padding: 6px 8px; color: #1a1a1a; border: none; outline: none; }
    QTreeWidget::item:hover { background-color: #e8eef4; border: none; outline: none; }
    QTreeWidget::item:selected { background-color: #dce8f2; color: #1a1a1a; border: none; outline: none; }
    QTreeWidget::item:focus { border: none; outline: none; }
    QHeaderView::section { padding: 8px; background-color: #ecf0f1; color: #2c3e50; }
    QCheckBox { spacing: 6px; }
    QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }
    QCheckBox::indicator:checked { background-color: #27ae60; border-color: #1e8449; image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDE4IDE4Ij48cGF0aCBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIiBkPSJNMyA5bDQgNCA4LTEyIi8+PC9zdmc+); }
    QCheckBox::indicator:hover { border-color: #27ae60; }
)";

static QTreeWidget* createAccionesTree(QWidget* parent)
{
    auto* tree = new QTreeWidget(parent);
    tree->setStyleSheet(QString::fromUtf8(treeItemStyle));
    tree->setColumnCount(4);
    tree->setHeaderLabels({ parent->tr("Acci\u00f3n / Par\u00e1metro"), parent->tr("Tipo"),
                            parent->tr("Opcional"), parent->tr("Descripci\u00f3n") });
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(true);
    tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    tree->setMinimumHeight(180);
    tree->setMaximumHeight(300);
    tree->setColumnWidth(0, 160);
    tree->setColumnWidth(1, 90);
    tree->setColumnWidth(2, 70);
    tree->header()->setStretchLastSection(true);
    return tree;
}

static void addAccionToTree(QTreeWidget* tree)
{
    auto* item = new QTreeWidgetItem(tree, { tree->tr("Nueva acci\u00f3n") });
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setExpanded(true);
    tree->setCurrentItem(item);
}

static void addParamToTree(QTreeWidget* tree)
{
    QTreeWidgetItem* accItem = tree->currentItem();
    if (!accItem) return;
    if (accItem->parent())
        accItem = accItem->parent();
    auto* child = new QTreeWidgetItem(accItem, { QString() });
    child->setFlags(child->flags() | Qt::ItemIsEditable);
    auto* combo = new QComboBox(tree);
    combo->addItems(VAR_TIPOS);
    auto* optCheck = new QCheckBox(tree);
    auto* descEdit = new QLineEdit(tree);
    descEdit->setPlaceholderText(tree->tr("Descripci\u00f3n"));
    tree->setItemWidget(child, 1, combo);
    tree->setItemWidget(child, 2, optCheck);
    tree->setItemWidget(child, 3, descEdit);
    tree->setCurrentItem(child);
    tree->editItem(child, 0);
}

static void removeFromAccionesTree(QTreeWidget* tree)
{
    QTreeWidgetItem* item = tree->currentItem();
    if (!item) return;
    QTreeWidgetItem* parent = item->parent();
    if (parent)
        parent->removeChild(item);
    else
        tree->takeTopLevelItem(tree->indexOfTopLevelItem(item));
    delete item;
}

QString TiposPage::buildParamsJson(QTableWidget* varsTable)
{
    QJsonObject root;
    for (int r = 0; r < varsTable->rowCount(); ++r) {
        QTableWidgetItem* nameItem = varsTable->item(r, 0);
        QWidget* w = varsTable->cellWidget(r, 1);
        auto* combo = qobject_cast<QComboBox*>(w);
        if (!nameItem || !combo) continue;
        QString name = nameItem->text().trimmed();
        if (name.isEmpty()) continue;
        QJsonObject varDef;
        varDef.insert(QStringLiteral("tipo"), combo->currentText());
        root.insert(name, varDef);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void TiposPage::fillParamsTable(const QString& paramsJson, QTableWidget* varsTable)
{
    varsTable->setRowCount(0);
    if (paramsJson.isEmpty() || paramsJson == QLatin1String("{}")) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString varName = it.key();
        QString tipo = QStringLiteral("texto");
        if (it.value().isObject()) {
            QJsonObject v = it.value().toObject();
            if (v.contains(QStringLiteral("tipo")))
                tipo = v[QStringLiteral("tipo")].toString();
        }
        int row = varsTable->rowCount();
        varsTable->insertRow(row);
        varsTable->setItem(row, 0, new QTableWidgetItem(varName));
        auto* combo = new QComboBox(varsTable);
        combo->addItems(VAR_TIPOS);
        int idx = VAR_TIPOS.indexOf(tipo);
        if (idx >= 0) combo->setCurrentIndex(idx);
        varsTable->setCellWidget(row, 1, combo);
    }
}

void TiposPage::refreshTable()
{
    m_overlay->showOverlay();
    m_service->fetchAll();
}

static QTableWidget* createParamsEditor(QWidget* parent, const QString& col0Header, const QString& col1Header)
{
    auto* table = new QTableWidget(parent);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({ col0Header, col1Header });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumHeight(120);
    table->setMaximumHeight(200);
    return table;
}

static void addVarRow(QTableWidget* table)
{
    int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(QString()));
    auto* combo = new QComboBox(table);
    combo->addItems(VAR_TIPOS);
    table->setCellWidget(row, 1, combo);
}

void TiposPage::onAdd()
{
    if (m_viewOnly) return;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("A\u00f1adir tipo"));
    dlg.setMinimumWidth(640);
    auto* form = new QFormLayout(&dlg);
    auto* nombreEdit = new QLineEdit(&dlg);
    nombreEdit->setPlaceholderText(tr("Nombre del tipo"));
    nombreEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nombreEdit);

    auto* accGroup = new QGroupBox(tr("Acciones del tipo"), &dlg);
    auto* accLayout = new QVBoxLayout(accGroup);
    auto* accTree = createAccionesTree(&dlg);
    accLayout->addWidget(accTree);
    auto* accBtnLayout = new QHBoxLayout();
    auto* addAccionBtn = new QPushButton(tr("A\u00f1adir acci\u00f3n"), &dlg);
    auto* addParamBtn = new QPushButton(tr("A\u00f1adir par\u00e1metro"), &dlg);
    auto* removeAccBtn = new QPushButton(tr("Quitar"), &dlg);
    accBtnLayout->addWidget(addAccionBtn);
    accBtnLayout->addWidget(addParamBtn);
    accBtnLayout->addWidget(removeAccBtn);
    accBtnLayout->addStretch(1);
    accLayout->addLayout(accBtnLayout);
    form->addRow(accGroup);

    auto* varsGroup = new QGroupBox(tr("Variables del tipo (params)"), &dlg);
    auto* varsLayout = new QVBoxLayout(varsGroup);
    auto* varsTable = createParamsEditor(&dlg, tr("Nombre variable"), tr("Tipo"));
    varsLayout->addWidget(varsTable);
    auto* varBtnLayout = new QHBoxLayout();
    auto* addVarBtn = new QPushButton(tr("A\u00f1adir variable"), &dlg);
    auto* removeVarBtn = new QPushButton(tr("Quitar variable"), &dlg);
    varBtnLayout->addWidget(addVarBtn);
    varBtnLayout->addWidget(removeVarBtn);
    varBtnLayout->addStretch(1);
    varsLayout->addLayout(varBtnLayout);
    form->addRow(varsGroup);

    connect(addAccionBtn, &QPushButton::clicked, &dlg, [accTree]() { addAccionToTree(accTree); });
    connect(addParamBtn, &QPushButton::clicked, &dlg, [accTree]() { addParamToTree(accTree); });
    connect(removeAccBtn, &QPushButton::clicked, &dlg, [accTree]() { removeFromAccionesTree(accTree); });
    connect(addVarBtn, &QPushButton::clicked, &dlg, [varsTable]() { addVarRow(varsTable); });
    connect(removeVarBtn, &QPushButton::clicked, &dlg, [varsTable]() {
        int row = varsTable->currentRow();
        if (row >= 0) varsTable->removeRow(row);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString nombre = nombreEdit->text().trimmed();
    if (nombre.isEmpty()) {
        QMessageBox::warning(this, tr("Tipos"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    QString acciones = buildAccionesJson(accTree);
    QString params = buildParamsJson(varsTable);
    m_overlay->showOverlay();
    m_service->create(nombre, acciones, params);
}

void TiposPage::onEdit()
{
    if (m_viewOnly) return;
    int id = selectedId();
    if (id <= 0) return;
    int row = m_table->currentRow();
    QString currentNombre = m_table->item(row, 1)->text();
    QString currentAcciones;
    QString currentParams;
    QTableWidgetItem* idItem = m_table->item(row, 0);
    if (idItem) {
        currentAcciones = idItem->data(Qt::UserRole).toString();
        currentParams = idItem->data(Qt::UserRole + 1).toString();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Editar tipo"));
    dlg.setMinimumWidth(640);
    auto* form = new QFormLayout(&dlg);
    auto* nombreEdit = new QLineEdit(&dlg);
    nombreEdit->setText(currentNombre);
    nombreEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nombreEdit);

    auto* accGroup = new QGroupBox(tr("Acciones del tipo"), &dlg);
    auto* accLayout = new QVBoxLayout(accGroup);
    auto* accTree = createAccionesTree(&dlg);
    fillAccionesTree(currentAcciones, accTree);
    accLayout->addWidget(accTree);
    auto* accBtnLayout = new QHBoxLayout();
    auto* addAccionBtn = new QPushButton(tr("A\u00f1adir acci\u00f3n"), &dlg);
    auto* addParamBtn = new QPushButton(tr("A\u00f1adir par\u00e1metro"), &dlg);
    auto* removeAccBtn = new QPushButton(tr("Quitar"), &dlg);
    accBtnLayout->addWidget(addAccionBtn);
    accBtnLayout->addWidget(addParamBtn);
    accBtnLayout->addWidget(removeAccBtn);
    accBtnLayout->addStretch(1);
    accLayout->addLayout(accBtnLayout);
    form->addRow(accGroup);

    auto* varsGroup = new QGroupBox(tr("Variables del tipo (params)"), &dlg);
    auto* varsLayout = new QVBoxLayout(varsGroup);
    auto* varsTable = createParamsEditor(&dlg, tr("Nombre variable"), tr("Tipo"));
    fillParamsTable(currentParams, varsTable);
    varsLayout->addWidget(varsTable);
    auto* varBtnLayout = new QHBoxLayout();
    auto* addVarBtn = new QPushButton(tr("A\u00f1adir variable"), &dlg);
    auto* removeVarBtn = new QPushButton(tr("Quitar variable"), &dlg);
    varBtnLayout->addWidget(addVarBtn);
    varBtnLayout->addWidget(removeVarBtn);
    varBtnLayout->addStretch(1);
    varsLayout->addLayout(varBtnLayout);
    form->addRow(varsGroup);

    connect(addAccionBtn, &QPushButton::clicked, &dlg, [accTree]() { addAccionToTree(accTree); });
    connect(addParamBtn, &QPushButton::clicked, &dlg, [accTree]() { addParamToTree(accTree); });
    connect(removeAccBtn, &QPushButton::clicked, &dlg, [accTree]() { removeFromAccionesTree(accTree); });
    connect(addVarBtn, &QPushButton::clicked, &dlg, [varsTable]() { addVarRow(varsTable); });
    connect(removeVarBtn, &QPushButton::clicked, &dlg, [varsTable]() {
        int r = varsTable->currentRow();
        if (r >= 0) varsTable->removeRow(r);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString nombre = nombreEdit->text().trimmed();
    if (nombre.isEmpty()) {
        QMessageBox::warning(this, tr("Tipos"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    QString acciones = buildAccionesJson(accTree);
    QString params = buildParamsJson(varsTable);
    m_overlay->showOverlay();
    m_service->update(id, nombre, acciones, params);
}

void TiposPage::onDelete()
{
    if (m_viewOnly) return;
    int id = selectedId();
    if (id <= 0) return;
    QString nombre = m_table->item(m_table->currentRow(), 1)->text();
    auto ret = QMessageBox::question(this, tr("Eliminar tipo"),
        tr("\u00bfEliminar \"%1\"?").arg(nombre),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_overlay->showOverlay();
    m_service->remove(id);
}

int TiposPage::selectedId() const
{
    int row = m_table->currentRow();
    if (row < 0) return 0;
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->text().toInt() : 0;
}
