#include "PuertosPage.h"
#include "core/SupabaseClient.h"
#include "core/PuertoService.h"
#include "core/TipoService.h"
#include "ui/LoadingOverlay.h"
#include "models/Puerto.h"
#include "models/Tipo.h"
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
#include <QSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QItemSelectionModel>
#include <algorithm>

static const QStringList VAR_TIPOS = { QStringLiteral("entero"), QStringLiteral("texto"), QStringLiteral("decimal"), QStringLiteral("booleano") };
static constexpr int kNombreTipoColumnWidth = 200;

PuertosPage::PuertosPage(SupabaseClient* supabase, HistorialService* historial, bool viewOnly,
                         QWidget* parent)
    : QWidget(parent)
    , m_viewOnly(viewOnly)
    , m_supabase(supabase)
    , m_service(new PuertoService(supabase, historial, this))
    , m_tipoService(new TipoService(supabase, historial, this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(tr("Puertos"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    layout->addWidget(title);

    auto* desc = new QLabel(tr("Gestiona los puertos y sus par\u00e1metros (params)"), this);
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

    auto* filterSortLayout = new QHBoxLayout();
    filterSortLayout->addWidget(new QLabel(tr("Filtrar por tipo:"), this));
    m_tipoFilterCombo = new QComboBox(this);
    m_tipoFilterCombo->addItem(tr("Todos los tipos"), -1);
    m_tipoFilterCombo->setMinimumWidth(180);
    filterSortLayout->addWidget(m_tipoFilterCombo);
    filterSortLayout->addSpacing(20);
    filterSortLayout->addWidget(new QLabel(tr("Ordenar por:"), this));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("Nombre"), 1);
    m_sortCombo->addItem(tr("Tipo"), 2);
    m_sortCombo->addItem(tr("Pin"), 3);
    m_sortCombo->setMinimumWidth(120);
    filterSortLayout->addWidget(m_sortCombo);
    filterSortLayout->addStretch(1);
    layout->addLayout(filterSortLayout);
    layout->addSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ tr("Id"), tr("Nombre"), tr("Tipo"), tr("Pin"), tr("Params") });
    m_table->setColumnHidden(0, true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setColumnWidth(1, kNombreTipoColumnWidth);
    m_table->setColumnWidth(2, kNombreTipoColumnWidth);
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

    connect(addBtn, &QPushButton::clicked, this, &PuertosPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &PuertosPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PuertosPage::onDelete);
    if (!m_viewOnly)
        connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { onEdit(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        bool has = selectedId() > 0;
        const bool canEdit = !m_viewOnly && has;
        m_editBtn->setEnabled(canEdit);
        m_deleteBtn->setEnabled(canEdit);
    });
    connect(m_tipoFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PuertosPage::refreshTable);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PuertosPage::refreshTable);

    connect(m_service, &PuertoService::dataReady, this, [this](const QVector<Puerto>& list) {
        m_overlay->hideOverlay();
        QVector<Puerto> filtered = list;
        int tipoFilterId = m_tipoFilterCombo->currentData().toInt();
        if (tipoFilterId >= 0) {
            filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                [tipoFilterId](const Puerto& p) { return p.tipoId != tipoFilterId; }), filtered.end());
        }

        int sortBy = m_sortCombo->currentData().toInt();
        if (sortBy == 1)
            std::sort(filtered.begin(), filtered.end(), [](const Puerto& a, const Puerto& b) { return QString::compare(a.nombre, b.nombre, Qt::CaseInsensitive) < 0; });
        else if (sortBy == 2)
            std::sort(filtered.begin(), filtered.end(), [](const Puerto& a, const Puerto& b) {
                QString na = a.tipoNombre.isEmpty() ? QString::number(a.tipoId) : a.tipoNombre;
                QString nb = b.tipoNombre.isEmpty() ? QString::number(b.tipoId) : b.tipoNombre;
                return QString::compare(na, nb, Qt::CaseInsensitive) < 0;
            });
        else if (sortBy == 3)
            std::sort(filtered.begin(), filtered.end(), [](const Puerto& a, const Puerto& b) { return a.pin < b.pin; });

        m_table->setRowCount(filtered.size());
        for (int i = 0; i < filtered.size(); ++i) {
            m_table->setItem(i, 0, new QTableWidgetItem(QString::number(filtered[i].id)));
            m_table->setItem(i, 1, new QTableWidgetItem(filtered[i].nombre));
            m_table->setItem(i, 2, new QTableWidgetItem(filtered[i].tipoNombre.isEmpty() ? QString::number(filtered[i].tipoId) : filtered[i].tipoNombre));
            m_table->setItem(i, 3, new QTableWidgetItem(QString::number(filtered[i].pin)));
            m_table->setItem(i, 4, new QTableWidgetItem(paramsToSummary(filtered[i].params)));
        }
        m_countLabel->setText(tr("Mostrando %1 elementos").arg(filtered.size()));
    });
    connect(m_service, &PuertoService::mutationDone, this, &PuertosPage::refreshTable);
    connect(m_service, &PuertoService::errorOccurred, this, [this](const QString& msg) {
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Puertos"), msg);
    });

    connect(m_tipoService, &TipoService::dataReady, this, [this](const QVector<Tipo>& tipos) {
        m_tipoFilterCombo->blockSignals(true);
        m_tipoFilterCombo->clear();
        m_tipoFilterCombo->addItem(tr("Todos los tipos"), -1);
        for (const auto& t : tipos)
            m_tipoFilterCombo->addItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre, t.id);
        m_tipoFilterCombo->blockSignals(false);
    });
    connect(m_tipoService, &TipoService::errorOccurred, this, [this](const QString& msg) {
        QMessageBox::critical(this, tr("Puertos"), msg);
    });

    m_tipoService->fetchAll();
    refreshTable();
}

QString PuertosPage::paramsToSummary(const QString& paramsJson)
{
    if (paramsJson.isEmpty() || paramsJson == QLatin1String("{}")) return tr("—");
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return tr("—");
    QJsonObject obj = doc.object();
    if (obj.isEmpty()) return tr("—");
    QStringList names = obj.keys();
    return tr("%1 param(s): %2").arg(names.size()).arg(names.join(QStringLiteral(", ")));
}

QString PuertosPage::buildParamsJson(QTableWidget* varsTable)
{
    QJsonObject root;
    for (int r = 0; r < varsTable->rowCount(); ++r) {
        QTableWidgetItem* nameItem = varsTable->item(r, 0);
        QWidget* w1 = varsTable->cellWidget(r, 1);
        QWidget* w2 = varsTable->cellWidget(r, 2);
        auto* combo = qobject_cast<QComboBox*>(w1);
        auto* valorEdit = qobject_cast<QLineEdit*>(w2);
        if (!nameItem || !combo) continue;
        QString name = nameItem->text().trimmed();
        if (name.isEmpty()) continue;
        QJsonObject paramObj;
        paramObj.insert(QStringLiteral("tipo"), combo->currentText());
        paramObj.insert(QStringLiteral("valor"), valorEdit ? valorEdit->text().trimmed() : QString());
        root.insert(name, paramObj);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PuertosPage::fillParamsTable(const QString& paramsJson, QTableWidget* varsTable)
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
        QString valor;
        if (it.value().isString()) {
            tipo = it.value().toString();
        } else if (it.value().isObject()) {
            QJsonObject v = it.value().toObject();
            if (v.contains(QStringLiteral("tipo")))
                tipo = v[QStringLiteral("tipo")].toString();
            if (v.contains(QStringLiteral("valor")))
                valor = v[QStringLiteral("valor")].toString();
        }
        int row = varsTable->rowCount();
        varsTable->insertRow(row);
        varsTable->setItem(row, 0, new QTableWidgetItem(varName));
        auto* combo = new QComboBox(varsTable);
        combo->addItems(VAR_TIPOS);
        int idx = VAR_TIPOS.indexOf(tipo);
        if (idx >= 0) combo->setCurrentIndex(idx);
        varsTable->setCellWidget(row, 1, combo);
        auto* valorEdit = new QLineEdit(varsTable);
        valorEdit->setText(valor);
        valorEdit->setPlaceholderText(tr("Valor"));
        varsTable->setCellWidget(row, 2, valorEdit);
    }
}

static QTableWidget* createParamsEditor(QWidget* parent, const QString& col0Header, const QString& col1Header, const QString& col2Header)
{
    auto* table = new QTableWidget(parent);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ col0Header, col1Header, col2Header });
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
    auto* valorEdit = new QLineEdit(table);
    valorEdit->setPlaceholderText(table->tr("Valor"));
    table->setCellWidget(row, 2, valorEdit);
}

void PuertosPage::refreshTable()
{
    m_overlay->showOverlay();
    m_service->fetchAll();
}

void PuertosPage::onAdd()
{
    auto openAddDialog = [this](const QVector<Tipo>& tipos) {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("A\u00f1adir puerto"));
        dlg.setMinimumWidth(640);
        auto* form = new QFormLayout(&dlg);
        auto* nombreEdit = new QLineEdit(&dlg);
        nombreEdit->setPlaceholderText(tr("Nombre del puerto"));
        nombreEdit->setMinimumWidth(280);
        form->addRow(tr("Nombre:"), nombreEdit);

        auto* tipoCombo = new QComboBox(&dlg);
        for (const auto& t : tipos)
            tipoCombo->addItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre, t.id);
        tipoCombo->setMinimumWidth(200);
        form->addRow(tr("Tipo:"), tipoCombo);

        auto* pinSpin = new QSpinBox(&dlg);
        pinSpin->setRange(0, 9999);
        pinSpin->setValue(0);
        form->addRow(tr("Pin:"), pinSpin);

        auto* varsGroup = new QGroupBox(tr("Params (par\u00e1metro : tipo)"), &dlg);
        auto* varsLayout = new QVBoxLayout(varsGroup);
        auto* varsTable = createParamsEditor(&dlg, tr("Par\u00e1metro"), tr("Tipo"), tr("Valor"));
        varsLayout->addWidget(varsTable);
        auto* varBtnLayout = new QHBoxLayout();
        auto* addVarBtn = new QPushButton(tr("A\u00f1adir par\u00e1metro"), &dlg);
        auto* removeVarBtn = new QPushButton(tr("Quitar"), &dlg);
        varBtnLayout->addWidget(addVarBtn);
        varBtnLayout->addWidget(removeVarBtn);
        varBtnLayout->addStretch(1);
        varsLayout->addLayout(varBtnLayout);
        form->addRow(varsGroup);

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
            QMessageBox::warning(this, tr("Puertos"), tr("El nombre no puede estar vac\u00edo."));
            return;
        }
        int tipoId = tipoCombo->currentData().toInt();
        int pin = pinSpin->value();
        QString params = buildParamsJson(varsTable);
        m_overlay->showOverlay();
        m_service->create(nombre, tipoId, pin, params);
    };

    if (m_tipoFilterCombo->count() > 1) {
        QVector<Tipo> tipos;
        for (int i = 1; i < m_tipoFilterCombo->count(); ++i) {
            Tipo t;
            t.id = m_tipoFilterCombo->itemData(i).toInt();
            t.nombre = m_tipoFilterCombo->itemText(i);
            tipos.append(t);
        }
        openAddDialog(tipos);
        return;
    }
    m_overlay->showOverlay();
    auto* pConn = new QMetaObject::Connection;
    auto* pErr = new QMetaObject::Connection;
    *pConn = connect(m_tipoService, &TipoService::dataReady, this, [this, openAddDialog, pConn, pErr](const QVector<Tipo>& tipos) {
        disconnect(*pConn);
        disconnect(*pErr);
        delete pConn;
        delete pErr;
        m_overlay->hideOverlay();
        openAddDialog(tipos);
    });
    *pErr = connect(m_tipoService, &TipoService::errorOccurred, this, [this, pConn, pErr](const QString& msg) {
        disconnect(*pConn);
        disconnect(*pErr);
        delete pConn;
        delete pErr;
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Puertos"), msg);
    });
    m_tipoService->fetchAll();
}

void PuertosPage::onEdit()
{
    if (m_viewOnly) return;
    int id = selectedId();
    if (id <= 0) return;
    m_overlay->showOverlay();
    auto* pConn = new QMetaObject::Connection;
    auto* pErr = new QMetaObject::Connection;
    *pConn = connect(m_service, &PuertoService::dataReady, this, [this, id, pConn, pErr](const QVector<Puerto>& list) {
        disconnect(*pConn);
        disconnect(*pErr);
        delete pConn;
        delete pErr;
        const Puerto* current = nullptr;
        for (const auto& p : list) {
            if (p.id == id) { current = &p; break; }
        }
        if (!current) {
            m_overlay->hideOverlay();
            return;
        }
        Puerto puertoCopy = *current;

        auto openEditDialog = [this, puertoCopy](const QVector<Tipo>& tipos) {
            QDialog dlg(this);
            dlg.setWindowTitle(tr("Editar puerto"));
            dlg.setMinimumWidth(640);
            auto* form = new QFormLayout(&dlg);
            auto* nombreEdit = new QLineEdit(&dlg);
            nombreEdit->setText(puertoCopy.nombre);
            nombreEdit->setMinimumWidth(280);
            form->addRow(tr("Nombre:"), nombreEdit);

            auto* tipoCombo = new QComboBox(&dlg);
            int tipoIdx = 0;
            for (int i = 0; i < tipos.size(); ++i) {
                tipoCombo->addItem(tipos[i].nombre.isEmpty() ? QString::number(tipos[i].id) : tipos[i].nombre, tipos[i].id);
                if (tipos[i].id == puertoCopy.tipoId) tipoIdx = i;
            }
            tipoCombo->setCurrentIndex(tipoIdx);
            tipoCombo->setMinimumWidth(200);
            form->addRow(tr("Tipo:"), tipoCombo);

            auto* pinSpin = new QSpinBox(&dlg);
            pinSpin->setRange(0, 9999);
            pinSpin->setValue(puertoCopy.pin);
            form->addRow(tr("Pin:"), pinSpin);

            auto* varsGroup = new QGroupBox(tr("Params (par\u00e1metro : tipo)"), &dlg);
            auto* varsLayout = new QVBoxLayout(varsGroup);
            auto* varsTable = createParamsEditor(&dlg, tr("Par\u00e1metro"), tr("Tipo"), tr("Valor"));
            fillParamsTable(puertoCopy.params, varsTable);
            varsLayout->addWidget(varsTable);
            auto* varBtnLayout = new QHBoxLayout();
            auto* addVarBtn = new QPushButton(tr("A\u00f1adir par\u00e1metro"), &dlg);
            auto* removeVarBtn = new QPushButton(tr("Quitar"), &dlg);
            varBtnLayout->addWidget(addVarBtn);
            varBtnLayout->addWidget(removeVarBtn);
            varBtnLayout->addStretch(1);
            varsLayout->addLayout(varBtnLayout);
            form->addRow(varsGroup);

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
                QMessageBox::warning(this, tr("Puertos"), tr("El nombre no puede estar vac\u00edo."));
                return;
            }
            int tipoId = tipoCombo->currentData().toInt();
            int pin = pinSpin->value();
            QString params = buildParamsJson(varsTable);
            m_overlay->showOverlay();
            m_service->update(puertoCopy.id, nombre, tipoId, pin, params);
        };

        if (m_tipoFilterCombo->count() > 1) {
            QVector<Tipo> tipos;
            for (int i = 1; i < m_tipoFilterCombo->count(); ++i) {
                Tipo t;
                t.id = m_tipoFilterCombo->itemData(i).toInt();
                t.nombre = m_tipoFilterCombo->itemText(i);
                tipos.append(t);
            }
            m_overlay->hideOverlay();
            openEditDialog(tipos);
            return;
        }
        m_overlay->showOverlay();
        auto* pConn2 = new QMetaObject::Connection;
        auto* pErr2 = new QMetaObject::Connection;
        *pConn2 = connect(m_tipoService, &TipoService::dataReady, this, [this, openEditDialog, pConn2, pErr2](const QVector<Tipo>& tipos) {
            disconnect(*pConn2);
            disconnect(*pErr2);
            delete pConn2;
            delete pErr2;
            m_overlay->hideOverlay();
            openEditDialog(tipos);
        });
        *pErr2 = connect(m_tipoService, &TipoService::errorOccurred, this, [this, pConn2, pErr2](const QString& msg) {
            disconnect(*pConn2);
            disconnect(*pErr2);
            delete pConn2;
            delete pErr2;
            m_overlay->hideOverlay();
            QMessageBox::critical(this, tr("Puertos"), msg);
        });
        m_tipoService->fetchAll();
    });
    *pErr = connect(m_service, &PuertoService::errorOccurred, this, [this, pConn, pErr](const QString& msg) {
        disconnect(*pConn);
        disconnect(*pErr);
        delete pConn;
        delete pErr;
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Puertos"), msg);
    });
    m_service->fetchAll();
}

void PuertosPage::onDelete()
{
    if (m_viewOnly) return;
    int id = selectedId();
    if (id <= 0) return;
    QString nombre = m_table->item(m_table->currentRow(), 1)->text();
    auto ret = QMessageBox::question(this, tr("Eliminar puerto"),
        tr("\u00bfEliminar \"%1\"?").arg(nombre),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_overlay->showOverlay();
    m_service->remove(id);
}

int PuertosPage::selectedId() const
{
    int row = m_table->currentRow();
    if (row < 0) return 0;
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->text().toInt() : 0;
}
