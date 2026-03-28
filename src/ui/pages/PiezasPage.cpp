#include "PiezasPage.h"
#include "core/TipoService.h"
#include "core/SupabaseClient.h"
#include "core/PiezaService.h"
#include "core/PropietarioService.h"
#include "core/UbicacionService.h"
#include "core/TipoService.h"
#include "ui/LoadingOverlay.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QStackedWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QByteArray>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QListWidget>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QMouseEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QSignalBlocker>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QVariant>
#include <QSpinBox>
#include <QListWidgetItem>
#include <QHash>
#include <QMap>
#include <QSet>
#include <memory>

namespace {

static QJsonObject parseTipoVarDefs(const QString& paramsJson)
{
    if (paramsJson.isEmpty() || paramsJson == QLatin1String("{}")) return {};
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

static QString varTipoFromDef(const QJsonValue& defVal)
{
    if (defVal.isObject()) {
        QString t = defVal.toObject().value(QStringLiteral("tipo")).toString();
        if (!t.isEmpty()) return t;
    }
    return QStringLiteral("texto");
}

static void clearVBoxLayout(QLayout* lay)
{
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (QWidget* w = it->widget())
            w->deleteLater();
        delete it;
    }
}

static QWidget* makeParamValueEditor(const QString& varTipo, const QJsonValue& initial, QWidget* parent)
{
    QString t = varTipo.isEmpty() ? QStringLiteral("texto") : varTipo;
    if (t == QLatin1String("entero")) {
        auto* s = new QSpinBox(parent);
        s->setRange(-2000000000, 2000000000);
        s->setProperty("paramKind", QVariant(QStringLiteral("entero")));
        if (initial.isDouble())
            s->setValue(static_cast<int>(initial.toDouble()));
        else if (initial.isString())
            s->setValue(initial.toString().toInt());
        return s;
    }
    if (t == QLatin1String("decimal")) {
        auto* d = new QDoubleSpinBox(parent);
        d->setRange(-1e9, 1e9);
        d->setDecimals(6);
        d->setProperty("paramKind", QVariant(QStringLiteral("decimal")));
        if (initial.isDouble())
            d->setValue(initial.toDouble());
        else if (initial.isString())
            d->setValue(initial.toString().toDouble());
        return d;
    }
    if (t == QLatin1String("booleano")) {
        auto* c = new QCheckBox(parent);
        c->setProperty("paramKind", QVariant(QStringLiteral("booleano")));
        if (initial.isBool())
            c->setChecked(initial.toBool());
        else if (initial.isDouble())
            c->setChecked(initial.toInt() != 0);
        else if (initial.isString()) {
            QString u = initial.toString().toLower();
            c->setChecked(u == QLatin1String("1") || u == QLatin1String("true") || u == QLatin1String("si"));
        }
        return c;
    }
    auto* e = new QLineEdit(parent);
    e->setProperty("paramKind", QVariant(QStringLiteral("texto")));
    if (initial.isString())
        e->setText(initial.toString());
    else if (initial.isDouble())
        e->setText(QString::number(initial.toDouble()));
    else if (initial.isBool())
        e->setText(initial.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    return e;
}

static QJsonValue widgetValueToJson(QWidget* w)
{
    QString kind = w->property("paramKind").toString();
    if (kind == QLatin1String("entero")) {
        if (auto* s = qobject_cast<QSpinBox*>(w))
            return s->value();
    } else if (kind == QLatin1String("decimal")) {
        if (auto* d = qobject_cast<QDoubleSpinBox*>(w))
            return d->value();
    } else if (kind == QLatin1String("booleano")) {
        if (auto* c = qobject_cast<QCheckBox*>(w))
            return c->isChecked();
    } else if (auto* e = qobject_cast<QLineEdit*>(w)) {
        return e->text();
    }
    return QJsonValue();
}

static QJsonObject gatherPiezaParamValues(const QHash<int, QMap<QString, QWidget*>>& map)
{
    QJsonObject root;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QJsonObject sub;
        for (auto pit = it.value().constBegin(); pit != it.value().constEnd(); ++pit)
            sub.insert(pit.key(), widgetValueToJson(pit.value()));
        if (!sub.isEmpty())
            root.insert(QString::number(it.key()), sub);
    }
    return root;
}

static void pruneWorkingToChecked(QJsonObject* working, const QVector<int>& checkedIds)
{
    QSet<QString> ok;
    for (int id : checkedIds)
        ok.insert(QString::number(id));
    QJsonObject out;
    for (auto wit = working->begin(); wit != working->end(); ++wit) {
        if (ok.contains(wit.key()))
            out.insert(wit.key(), wit.value());
    }
    *working = out;
}

static void rebuildPiezaTipoParamsPanel(QWidget* trContext,
                                        QVBoxLayout* innerLay,
                                        const QVector<int>& checkedTipoIds,
                                        const QVector<Tipo>& tipos,
                                        const QJsonObject& workingParams,
                                        QHash<int, QMap<QString, QWidget*>>* outMap)
{
    clearVBoxLayout(innerLay);
    outMap->clear();
    for (int tipoId : checkedTipoIds) {
        const Tipo* tp = nullptr;
        for (const Tipo& t : tipos) {
            if (t.id == tipoId) {
                tp = &t;
                break;
            }
        }
        if (!tp) continue;
        QJsonObject varDefs = parseTipoVarDefs(tp->params);
        if (varDefs.isEmpty()) continue;
        auto* gb = new QGroupBox(tp->nombre.isEmpty() ? QStringLiteral("Tipo %1").arg(tipoId) : tp->nombre);
        auto* fl = new QFormLayout(gb);
        QJsonObject exist = workingParams.value(QString::number(tipoId)).toObject();
        QMap<QString, QWidget*> rowW;
        for (auto dit = varDefs.begin(); dit != varDefs.end(); ++dit) {
            QString varName = dit.key();
            QString vtipo = varTipoFromDef(dit.value());
            QWidget* ed = makeParamValueEditor(vtipo, exist.value(varName), gb);
            fl->addRow(varName + QLatin1Char(':'), ed);
            rowW.insert(varName, ed);
        }
        innerLay->addWidget(gb);
        (*outMap)[tipoId] = rowW;
    }
    if (innerLay->count() == 0 && trContext) {
        auto* hint = new QLabel(trContext->tr(
            "Marca tipos que definan variables (en Tipos) para rellenar sus valores aqu\u00ed."));
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
        innerLay->addWidget(hint);
    }
}

static QVector<int> checkedTipoIdsFromList(QListWidget* tiposList)
{
    QVector<int> ids;
    for (int i = 0; i < tiposList->count(); ++i) {
        if (tiposList->item(i)->checkState() == Qt::Checked)
            ids.append(tiposList->item(i)->data(Qt::UserRole).toInt());
    }
    return ids;
}

static QJsonObject workingParamsFromPieza(const Pieza& p)
{
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(p.paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !d.isObject()) return {};
    return d.object();
}

static QString piezaParamsToJsonString(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

static const int CARD_WIDTH = 230;
static const int CARD_HEIGHT = 300;
static const int IMG_WIDTH = 210;
static const int IMG_HEIGHT = 200;
static const int COLS = 5;

static QPixmap pixmapFromBase64(const QString& base64, int maxW, int maxH)
{
    if (base64.isEmpty()) return QPixmap();
    QByteArray ba = QByteArray::fromBase64(base64.toUtf8());
    if (ba.size() > 10 * 1024 * 1024) return QPixmap(); // evitar imágenes enormes
    QPixmap p;
    if (!p.loadFromData(ba)) return QPixmap();
    if (p.isNull()) return QPixmap();
    return p.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

PiezasPage::PiezasPage(SupabaseClient* supabase, HistorialService* historial, QWidget* parent)
    : QWidget(parent)
    , m_supabase(supabase)
    , m_piezaService(new PiezaService(supabase, historial, this))
    , m_propService(new PropietarioService(supabase, historial, this))
    , m_ubicService(new UbicacionService(supabase, historial, this))
    , m_tipoService(new TipoService(supabase, historial, this))
    , m_overlay(new LoadingOverlay(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* titleRow = new QHBoxLayout();
    auto* title = new QLabel(tr("Piezas"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    auto* addBtn = new QPushButton(tr("A\u00f1adir pieza"), this);
    addBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2ecc71; }"));
    addBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn = new QPushButton(tr("Editar"), this);
    m_dupBtn = new QPushButton(tr("Duplicar"), this);
    m_delBtn = new QPushButton(tr("Eliminar"), this);
    m_editBtn->setEnabled(false);
    m_dupBtn->setEnabled(false);
    m_delBtn->setEnabled(false);
    m_editBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e67e22; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #f39c12; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"));
    m_dupBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3498db; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5dade2; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"));
    m_delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #c0392b; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e74c3c; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"));
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_dupBtn->setCursor(Qt::PointingHandCursor);
    m_delBtn->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(addBtn);
    titleRow->addWidget(m_editBtn);
    titleRow->addWidget(m_dupBtn);
    titleRow->addWidget(m_delBtn);
    layout->addLayout(titleRow);

    connect(addBtn, &QPushButton::clicked, this, &PiezasPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, [this]() { if (m_selectedId > 0) onEdit(m_selectedId); });
    connect(m_dupBtn, &QPushButton::clicked, this, [this]() { if (m_selectedId > 0) onDuplicate(m_selectedId); });
    connect(m_delBtn, &QPushButton::clicked, this, [this]() { if (m_selectedId > 0) onDelete(m_selectedId); });

    auto* desc = new QLabel(tr("Gestiona las piezas del bel\u00e9n"), this);
    desc->setStyleSheet(QStringLiteral("font-size: 13px; color: #7f8c8d;"));
    layout->addWidget(desc);
    layout->addSpacing(12);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(10);

    static const char* filterStyle = "QLineEdit, QComboBox { padding: 6px 10px; border: 1px solid #dcdfe6; "
        "border-radius: 6px; background: #fff; font-size: 13px; min-height: 20px; }"
        "QLineEdit:focus, QComboBox:focus { border-color: #3498db; outline: none; }"
        "QComboBox::drop-down { border: none; padding-right: 6px; }"
        "QComboBox:hover { border-color: #bdc3c7; }";

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Buscar por nombre..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(180);
    m_searchEdit->setStyleSheet(QString::fromUtf8(filterStyle));
    filterRow->addWidget(m_searchEdit);

    m_tipoFilterCombo = new QComboBox(this);
    m_tipoFilterCombo->addItem(tr("Tipo: Todos"), 0);
    m_propietarioFilterCombo = new QComboBox(this);
    m_propietarioFilterCombo->addItem(tr("Propietario: Todos"), 0);
    m_ubicacionFilterCombo = new QComboBox(this);
    m_ubicacionFilterCombo->addItem(tr("Ubicaci\u00f3n: Todas"), 0);
    m_ubicacionFilterCombo->addItem(tr("Sin ubicaci\u00f3n"), -1);
    m_tipoFilterCombo->setMinimumWidth(120);
    m_propietarioFilterCombo->setMinimumWidth(140);
    m_ubicacionFilterCombo->setMinimumWidth(120);
    m_tipoFilterCombo->setStyleSheet(QString::fromUtf8(filterStyle));
    m_propietarioFilterCombo->setStyleSheet(QString::fromUtf8(filterStyle));
    m_ubicacionFilterCombo->setStyleSheet(QString::fromUtf8(filterStyle));
    filterRow->addWidget(m_tipoFilterCombo);
    filterRow->addWidget(m_propietarioFilterCombo);
    filterRow->addWidget(m_ubicacionFilterCombo);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("C\u00f3digo"), QStringLiteral("codigo"));
    m_sortCombo->addItem(tr("Nombre"), QStringLiteral("nombre"));
    m_sortCombo->addItem(tr("Notas"), QStringLiteral("notas"));
    m_sortCombo->addItem(tr("Fecha"), QStringLiteral("fecha"));
    m_sortCombo->addItem(tr("Precio"), QStringLiteral("precio"));
    m_sortCombo->addItem(tr("Propietario"), QStringLiteral("propietario_id"));
    m_sortCombo->addItem(tr("Ubicaci\u00f3n"), QStringLiteral("ubicacion_id"));
    m_sortOrderCombo = new QComboBox(this);
    m_sortOrderCombo->addItem(tr("Descendente"), static_cast<int>(Qt::DescendingOrder));
    m_sortOrderCombo->addItem(tr("Ascendente"), static_cast<int>(Qt::AscendingOrder));
    m_sortCombo->setMinimumWidth(100);
    m_sortOrderCombo->setMinimumWidth(100);
    m_sortCombo->setStyleSheet(QString::fromUtf8(filterStyle));
    m_sortOrderCombo->setStyleSheet(QString::fromUtf8(filterStyle));
    auto* ordenLbl = new QLabel(tr("Orden:"), this);
    ordenLbl->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 13px;"));
    filterRow->addWidget(ordenLbl);
    filterRow->addWidget(m_sortCombo);
    filterRow->addWidget(m_sortOrderCombo);
    filterRow->addStretch(1);
    layout->addLayout(filterRow);
    layout->addSpacing(8);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &PiezasPage::refreshCards);
    connect(m_tipoFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PiezasPage::refreshCards);
    connect(m_propietarioFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PiezasPage::refreshCards);
    connect(m_ubicacionFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PiezasPage::refreshCards);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PiezasPage::refreshCards);
    connect(m_sortOrderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PiezasPage::refreshCards);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    m_cardsContainer = new QWidget(this);
    m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_scroll->setWidget(m_cardsContainer);
    layout->addWidget(m_scroll, 1);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    layout->addWidget(m_countLabel, 0, Qt::AlignRight);

    m_hoverPopup = nullptr;
    m_hoverCard = nullptr;
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, &PiezasPage::onHoverTimerFired);

    m_countLabel->setText(tr("Mostrando 0 elementos"));

    connect(m_propService, &PropietarioService::dataReady, this, [this](const QVector<Propietario>& items) {
        m_lastPropietarios = items;
        QSignalBlocker b(m_propietarioFilterCombo);
        m_propietarioFilterCombo->clear();
        m_propietarioFilterCombo->addItem(tr("Propietario: Todos"), 0);
        for (const auto& p : items)
            m_propietarioFilterCombo->addItem(p.nombre, p.id);
    });
    connect(m_ubicService, &UbicacionService::dataReady, this, [this](const QVector<Ubicacion>& items) {
        m_lastUbicaciones = items;
        QSignalBlocker b(m_ubicacionFilterCombo);
        m_ubicacionFilterCombo->clear();
        m_ubicacionFilterCombo->addItem(tr("Ubicaci\u00f3n: Todas"), 0);
        m_ubicacionFilterCombo->addItem(tr("Sin ubicaci\u00f3n"), -1);
        for (const auto& u : items)
            m_ubicacionFilterCombo->addItem(u.nombre, u.id);
    });
    connect(m_tipoService, &TipoService::dataReady, this, [this](const QVector<Tipo>& items) {
        m_lastTipos = items;
        QSignalBlocker b(m_tipoFilterCombo);
        m_tipoFilterCombo->clear();
        m_tipoFilterCombo->addItem(tr("Tipo: Todos"), 0);
        for (const auto& t : items)
            m_tipoFilterCombo->addItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre, t.id);
    });

    connect(m_piezaService, &PiezaService::piezasFetched, this, [this](const QVector<PiezaCardData>& cards) {
        m_overlay->hideOverlay();
        m_cardsData = cards;
        rebuildGrid();
    });
    connect(m_piezaService, &PiezaService::mutationDone, this, &PiezasPage::refreshCards);
    connect(m_piezaService, &PiezaService::errorOccurred, this, [this](const QString& msg) {
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Piezas"), msg);
    });

    if (m_supabase) {
        m_propService->fetchAll();
        m_ubicService->fetchAll();
        m_tipoService->fetchAll();
        QTimer::singleShot(0, this, &PiezasPage::refreshCards);
    }
}

void PiezasPage::withTiposRefreshed(std::function<void()> body)
{
    if (!m_tipoService) {
        if (body)
            body();
        return;
    }
    auto ran = std::make_shared<bool>(false);
    auto once = [ran, body]() {
        if (*ran)
            return;
        *ran = true;
        if (body)
            body();
    };
    connect(m_tipoService, &TipoService::dataReady, this, once, Qt::SingleShotConnection);
    connect(m_tipoService, &TipoService::errorOccurred, this, once, Qt::SingleShotConnection);
    m_tipoService->fetchAll();
}

PiezaListParams PiezasPage::currentParams() const
{
    PiezaListParams p;
    p.searchNombre = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    p.filterTipoId = m_tipoFilterCombo ? m_tipoFilterCombo->currentData().toInt() : 0;
    p.filterPropietarioId = m_propietarioFilterCombo ? m_propietarioFilterCombo->currentData().toInt() : 0;
    p.filterUbicacionId = m_ubicacionFilterCombo ? m_ubicacionFilterCombo->currentData().toInt() : 0;
    p.sortBy = m_sortCombo ? m_sortCombo->currentData().toString() : QStringLiteral("codigo");
    p.sortOrder = m_sortOrderCombo ? static_cast<Qt::SortOrder>(m_sortOrderCombo->currentData().toInt()) : Qt::DescendingOrder;
    return p;
}

void PiezasPage::rebuildGrid()
{
    QLayout* oldLayout = m_cardsContainer->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto* grid = new QGridLayout(m_cardsContainer);
    grid->setSpacing(20);

    if (!m_supabase) {
        m_countLabel->setText(tr("Mostrando 0 elementos"));
        m_cardsData.clear();
        m_selectedId = 0;
        return;
    }

    bool selectionValid = false;
    for (const auto& d : m_cardsData) {
        if (d.pieza.id == m_selectedId) { selectionValid = true; break; }
    }
    if (!selectionValid) m_selectedId = 0;

    for (int i = 0; i < m_cardsData.size(); ++i) {
        int row = i / COLS;
        int col = i % COLS;
        grid->addWidget(createCard(m_cardsContainer, m_cardsData[i], i), row, col, 1, 1, Qt::AlignTop | Qt::AlignLeft);
    }
    updateSelectionStyle();
    m_editBtn->setEnabled(m_selectedId > 0);
    m_dupBtn->setEnabled(m_selectedId > 0);
    m_delBtn->setEnabled(m_selectedId > 0);
    m_countLabel->setText(tr("Mostrando %1 elementos").arg(m_cardsData.size()));
}

void PiezasPage::refreshCards()
{
    if (!m_supabase) return;
    m_overlay->showOverlay();
    m_piezaService->fetchPiezasForCards(currentParams());
}

bool PiezasPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Enter) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->property("cardIndex").isValid()) {
            m_hoverCard = w;
            m_hoverTimer->start(400);
        }
    } else if (event->type() == QEvent::Leave) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w == m_hoverCard) {
            m_hoverTimer->stop();
            m_hoverCard = nullptr;
            hideHoverPopup();
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->property("piezaId").isValid()) {
            int id = w->property("piezaId").toInt();
            if (id > 0) {
                onCardSelected(id);
                onEdit(id);
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->property("piezaId").isValid()) {
            int id = w->property("piezaId").toInt();
            if (id > 0)
                onCardSelected(id);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PiezasPage::onCardSelected(int piezaId)
{
    m_selectedId = piezaId;
    updateSelectionStyle();
    m_editBtn->setEnabled(m_selectedId > 0);
    m_dupBtn->setEnabled(m_selectedId > 0);
    m_delBtn->setEnabled(m_selectedId > 0);
}

void PiezasPage::updateSelectionStyle()
{
    if (!m_cardsContainer || !m_cardsContainer->layout()) return;
    QGridLayout* grid = qobject_cast<QGridLayout*>(m_cardsContainer->layout());
    if (!grid) return;
    for (int i = 0; i < grid->count(); ++i) {
        QWidget* w = grid->itemAt(i)->widget();
        if (!w) continue;
        int pid = w->property("piezaId").toInt();
        bool sel = (pid == m_selectedId);
        w->setStyleSheet(sel
            ? QStringLiteral("QFrame { background-color: #e8f4fd; border: 2px solid #3498db; border-radius: 8px; }")
            : QStringLiteral("QFrame { background-color: white; border: 1px solid #e0e0e0; border-radius: 8px; } QFrame:hover { border-color: #bdc3c7; background-color: #f8f9fa; }"));
    }
}

void PiezasPage::onHoverTimerFired()
{
    if (!m_hoverCard || !m_hoverCard->property("cardIndex").isValid()) return;
    int idx = m_hoverCard->property("cardIndex").toInt();
    if (idx < 0 || idx >= m_cardsData.size()) return;
    QPoint pos = QCursor::pos();
    pos += QPoint(12, 12);
    showHoverPopup(m_cardsData[idx], pos);
}

void PiezasPage::showHoverPopup(const PiezaCardData& data, const QPoint& globalPos)
{
    hideHoverPopup();
    m_hoverPopup = new QFrame(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    m_hoverPopup->setAttribute(Qt::WA_TranslucentBackground, false);
    m_hoverPopup->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #fff; border: 1px solid #bdc3c7; border-radius: 4px; padding: 8px 12px; }"
        "QLabel { font-size: 12px; color: #2c3e50; background: transparent; border: none; padding: 0; }"));
    m_hoverPopup->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    auto* lay = new QVBoxLayout(m_hoverPopup);
    lay->setSpacing(2);
    lay->setContentsMargins(6, 6, 6, 6);
    auto addLine = [this, lay](const QString& label, const QString& value) {
        if (value.isEmpty()) return;
        auto* l = new QLabel(QStringLiteral("<b>%1:</b> %2").arg(label.toHtmlEscaped(), value.toHtmlEscaped()), m_hoverPopup);
        l->setFrameShape(QFrame::NoFrame);
        l->setStyleSheet(QStringLiteral("font-size: 12px; color: #2c3e50; background: transparent; border: none; padding: 0;"));
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        l->setMaximumWidth(320);
        lay->addWidget(l);
    };
    addLine(tr("C\u00f3digo"), data.pieza.codigo);
    addLine(tr("Nombre"), data.pieza.nombre);
    addLine(tr("Notas"), data.pieza.notas);
    if (data.pieza.fecha.isValid())
        addLine(tr("Fecha"), data.pieza.fecha.toString(Qt::ISODate));
    addLine(tr("Precio"), QString::number(data.pieza.precio, 'f', 2));
    addLine(tr("Propietario"), data.propietarioNombre);
    addLine(tr("Ubicaci\u00f3n"), data.ubicacionNombre);
    addLine(tr("Tipos"), data.tipos.join(QStringLiteral(", ")));
    m_hoverPopup->adjustSize();
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);
    int x = globalPos.x();
    int y = globalPos.y();
    if (x + m_hoverPopup->width() > screenRect.right())
        x = screenRect.right() - m_hoverPopup->width();
    if (y + m_hoverPopup->height() > screenRect.bottom())
        y = screenRect.bottom() - m_hoverPopup->height();
    if (x < screenRect.left()) x = screenRect.left();
    if (y < screenRect.top()) y = screenRect.top();
    m_hoverPopup->move(x, y);
    m_hoverPopup->show();
}

void PiezasPage::hideHoverPopup()
{
    if (m_hoverPopup) {
        m_hoverPopup->deleteLater();
        m_hoverPopup = nullptr;
    }
}

QWidget* PiezasPage::createCard(QWidget* parent, const PiezaCardData& data, int index)
{
    QFrame* card = new QFrame(parent);
    card->setFixedSize(CARD_WIDTH, CARD_HEIGHT);
    card->setProperty("cardIndex", index);
    card->setProperty("piezaId", data.pieza.id);
    card->setAttribute(Qt::WA_Hover, true);
    card->installEventFilter(this);
    card->setCursor(Qt::PointingHandCursor);
    bool sel = (data.pieza.id == m_selectedId);
    card->setStyleSheet(sel
        ? QStringLiteral("QFrame { background-color: #e8f4fd; border: 2px solid #3498db; border-radius: 8px; }")
        : QStringLiteral("QFrame { background-color: white; border: 1px solid #e0e0e0; border-radius: 8px; } QFrame:hover { border-color: #bdc3c7; background-color: #f8f9fa; }"));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(4);

    QWidget* imgArea = new QWidget(card);
    imgArea->setFixedSize(IMG_WIDTH, IMG_HEIGHT);
    imgArea->setStyleSheet(QStringLiteral("background-color: #ecf0f1; border-radius: 4px;"));
    auto* imgLayout = new QVBoxLayout(imgArea);
    imgLayout->setContentsMargins(0, 0, 0, 0);

    if (data.imagenesBase64.isEmpty()) {
        auto* placeholder = new QLabel(tr("Sin imagen"), imgArea);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QStringLiteral("color: #95a5a6; font-size: 12px; background: transparent;"));
        imgLayout->addWidget(placeholder, 1);
    } else {
        const int imgDisplayH = IMG_HEIGHT - 28;
        QStackedWidget* stack = new QStackedWidget(imgArea);
        stack->setFixedSize(IMG_WIDTH, imgDisplayH);
        for (const QString& b64 : data.imagenesBase64) {
            QPixmap px = pixmapFromBase64(b64, IMG_WIDTH, imgDisplayH);
            auto* lbl = new QLabel(imgArea);
            lbl->setFixedSize(IMG_WIDTH, imgDisplayH);
            if (!px.isNull())
                lbl->setPixmap(px);
            else
                lbl->setText(tr("Sin imagen"));
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setScaledContents(false);
            lbl->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
            stack->addWidget(lbl);
        }
        imgLayout->addWidget(stack, 1);

        if (data.imagenesBase64.size() > 1) {
            auto* navRow = new QHBoxLayout();
            auto* prevBtn = new QPushButton(tr("<"), imgArea);
            auto* nextBtn = new QPushButton(tr(">"), imgArea);
            prevBtn->setFixedSize(28, 24);
            nextBtn->setFixedSize(28, 24);
            prevBtn->setStyleSheet(QStringLiteral("QPushButton { background: #bdc3c7; border: none; border-radius: 2px; } QPushButton:hover { background: #95a5a6; }"));
            nextBtn->setStyleSheet(QStringLiteral("QPushButton { background: #bdc3c7; border: none; border-radius: 2px; } QPushButton:hover { background: #95a5a6; }"));
            navRow->addStretch(1);
            navRow->addWidget(prevBtn);
            navRow->addSpacing(4);
            navRow->addWidget(nextBtn);
            navRow->addStretch(1);
            imgLayout->addLayout(navRow);
            connect(prevBtn, &QPushButton::clicked, this, [stack]() {
                int i = stack->currentIndex();
                if (i > 0) stack->setCurrentIndex(i - 1);
            });
            connect(nextBtn, &QPushButton::clicked, this, [stack]() {
                int i = stack->currentIndex();
                if (i < stack->count() - 1) stack->setCurrentIndex(i + 1);
            });
        }
    }
    cardLayout->addWidget(imgArea, 0, Qt::AlignLeft);

    if (!data.pieza.codigo.isEmpty()) {
        auto* codigoLbl = new QLabel(data.pieza.codigo, card);
        codigoLbl->setFrameShape(QFrame::NoFrame);
        codigoLbl->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px; background: transparent; border: none; outline: none; margin-top: 6px;"));
        codigoLbl->setWordWrap(true);
        cardLayout->addWidget(codigoLbl);
    }
    auto* nombreLbl = new QLabel(data.pieza.nombre, card);
    nombreLbl->setFrameShape(QFrame::NoFrame);
    nombreLbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #2c3e50; background: transparent; border: none; outline: none;"));
    nombreLbl->setWordWrap(true);
    nombreLbl->setMaximumHeight(34);
    cardLayout->addWidget(nombreLbl);
    if (!data.tipos.isEmpty()) {
        auto* tipoLbl = new QLabel(data.tipos.join(QStringLiteral(", ")), card);
        tipoLbl->setFrameShape(QFrame::NoFrame);
        tipoLbl->setStyleSheet(QStringLiteral("color: #3498db; font-size: 11px; background: transparent; border: none; outline: none;"));
        tipoLbl->setWordWrap(true);
        tipoLbl->setMaximumHeight(28);
        cardLayout->addWidget(tipoLbl);
    }

    return card;
}

void PiezasPage::onAdd()
{
    if (!m_supabase) return;
    if (m_lastPropietarios.isEmpty()) {
        QMessageBox::warning(this, tr("Piezas"), tr("Crea al menos un propietario antes de a\u00f1adir piezas."));
        return;
    }

    withTiposRefreshed([this]() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("A\u00f1adir pieza"));
    dlg.setMinimumSize(780, 520);

    auto* mainLay = new QVBoxLayout(&dlg);
    auto* hRow = new QHBoxLayout();

    auto* leftCol = new QWidget(&dlg);
    leftCol->setMinimumWidth(280);
    auto* leftLay = new QVBoxLayout(leftCol);
    leftLay->setContentsMargins(0, 0, 10, 0);

    auto* tiposGroup = new QGroupBox(tr("Tipos"), &dlg);
    auto* tiposLayout = new QVBoxLayout(tiposGroup);
    auto* tiposList = new QListWidget(&dlg);
    tiposList->setMinimumHeight(0);
    tiposList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tiposList->setStyleSheet(QStringLiteral(
        "QListWidget::indicator { width: 18px; height: 18px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
        "QListWidget::indicator:checked { background-color: #27ae60; border-color: #1e8449; image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDE4IDE4Ij48cGF0aCBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIiBkPSJNMyA5bDQgNCA4LTEyIi8+PC9zdmc+); }"));
    for (const auto& t : m_lastTipos) {
        auto* item = new QListWidgetItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre);
        item->setData(Qt::UserRole, t.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        tiposList->addItem(item);
    }
    tiposLayout->addWidget(tiposList, 1);
    leftLay->addWidget(tiposGroup, 1);

    auto* paramsGroup = new QGroupBox(tr("Par\u00e1metros por tipo"), &dlg);
    auto* paramsGroupLay = new QVBoxLayout(paramsGroup);
    auto* paramsInner = new QWidget(paramsGroup);
    auto* paramsInnerLay = new QVBoxLayout(paramsInner);
    paramsInnerLay->setContentsMargins(4, 4, 4, 4);
    paramsGroupLay->addWidget(paramsInner);
    leftLay->addWidget(paramsGroup, 0);

    QJsonObject workingParams;
    QHash<int, QMap<QString, QWidget*>> paramWidgets;
    auto refreshParamPanel = [paramsInnerLay, tiposList, this, &workingParams, &paramWidgets]() {
        QJsonObject g = gatherPiezaParamValues(paramWidgets);
        for (auto it = g.begin(); it != g.end(); ++it)
            workingParams.insert(it.key(), it.value());
        QVector<int> ids = checkedTipoIdsFromList(tiposList);
        pruneWorkingToChecked(&workingParams, ids);
        rebuildPiezaTipoParamsPanel(this, paramsInnerLay, ids, m_lastTipos, workingParams, &paramWidgets);
    };
    QObject::connect(tiposList, &QListWidget::itemChanged, &dlg,
        [refreshParamPanel](QListWidgetItem*) { refreshParamPanel(); });
    refreshParamPanel();

    hRow->addWidget(leftCol, 0);

    auto* rightCol = new QWidget(&dlg);
    auto* rightOuter = new QVBoxLayout(rightCol);
    auto* rightForm = new QFormLayout();

    auto* codigoEdit = new QLineEdit(&dlg);
    codigoEdit->setPlaceholderText(tr("C\u00f3digo opcional"));
    rightForm->addRow(tr("C\u00f3digo:"), codigoEdit);

    auto* nombreEdit = new QLineEdit(&dlg);
    nombreEdit->setPlaceholderText(tr("Nombre de la pieza"));
    rightForm->addRow(tr("Nombre:"), nombreEdit);

    auto* notasEdit = new QLineEdit(&dlg);
    notasEdit->setPlaceholderText(tr("Notas opcionales"));
    rightForm->addRow(tr("Notas:"), notasEdit);

    auto* fechaCheck = new QCheckBox(tr("Usar fecha"), &dlg);
    fechaCheck->setChecked(false);
    fechaCheck->setStyleSheet(QStringLiteral(
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
        "QCheckBox::indicator:checked { background-color: #409eff; border-color: #2b7cd3; }"));
    auto* fechaEdit = new QDateEdit(&dlg);
    fechaEdit->setCalendarPopup(true);
    fechaEdit->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    fechaEdit->setDate(QDate::currentDate());
    fechaEdit->setEnabled(false);
    auto* fechaRow = new QHBoxLayout();
    fechaRow->addWidget(fechaCheck);
    fechaRow->addWidget(fechaEdit);
    rightForm->addRow(tr("Fecha:"), fechaRow);

    QObject::connect(fechaCheck, &QCheckBox::toggled, &dlg, [fechaEdit](bool on) {
        fechaEdit->setEnabled(on);
    });

    auto* precioSpin = new QDoubleSpinBox(&dlg);
    precioSpin->setRange(0, 999999.99);
    precioSpin->setDecimals(2);
    precioSpin->setValue(0);
    rightForm->addRow(tr("Precio:"), precioSpin);

    auto* propCombo = new QComboBox(&dlg);
    for (const auto& p : m_lastPropietarios)
        propCombo->addItem(p.nombre, p.id);
    rightForm->addRow(tr("Propietario:"), propCombo);

    auto* ubiCombo = new QComboBox(&dlg);
    ubiCombo->addItem(tr("Sin ubicaci\u00f3n"), 0);
    for (const auto& u : m_lastUbicaciones)
        ubiCombo->addItem(u.nombre, u.id);
    rightForm->addRow(tr("Ubicaci\u00f3n:"), ubiCombo);

    rightOuter->addLayout(rightForm);

    auto* imgsGroup = new QGroupBox(tr("Im\u00e1genes"), &dlg);
    auto* imgsLayout = new QVBoxLayout(imgsGroup);
    auto* imgsList = new QListWidget(&dlg);
    imgsList->setMinimumHeight(120);
    imgsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imgsList->setDragDropMode(QAbstractItemView::InternalMove);
    imgsList->setDefaultDropAction(Qt::MoveAction);
    imgsLayout->addWidget(imgsList);
    auto* addImgBtn = new QPushButton(tr("A\u00f1adir imagen..."), &dlg);
    auto* removeImgBtn = new QPushButton(tr("Quitar imagen"), &dlg);
    auto* imgsBtnRow = new QHBoxLayout();
    imgsBtnRow->addWidget(addImgBtn);
    imgsBtnRow->addWidget(removeImgBtn);
    imgsBtnRow->addStretch(1);
    imgsLayout->addLayout(imgsBtnRow);
    rightOuter->addWidget(imgsGroup, 1);

    hRow->addWidget(rightCol, 1);
    mainLay->addLayout(hRow, 1);

    connect(addImgBtn, &QPushButton::clicked, &dlg, [&dlg, imgsList]() {
        QString path = QFileDialog::getOpenFileName(&dlg, tr("Seleccionar imagen"), QString(),
            tr("Im\u00e1genes (*.png *.jpg *.jpeg *.bmp *.gif)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QByteArray ba = f.readAll().toBase64();
        f.close();
        if (ba.size() > 10 * 1024 * 1024) {
            QMessageBox::warning(&dlg, tr("Imagen"), tr("La imagen es demasiado grande (m\u00e1x. 10 MB)."));
            return;
        }
        auto* item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, QString::fromUtf8(ba));
        imgsList->addItem(item);
    });
    connect(removeImgBtn, &QPushButton::clicked, &dlg, [imgsList]() {
        int row = imgsList->currentRow();
        if (row >= 0) delete imgsList->takeItem(row);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    mainLay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [nombreEdit, &dlg]() {
        if (nombreEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, tr("Piezas"), tr("El nombre no puede estar vac\u00edo."));
            return;
        }
        dlg.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QVector<QString> imagenesBase64;
    for (int i = 0; i < imgsList->count(); ++i)
        imagenesBase64.append(imgsList->item(i)->data(Qt::UserRole).toString());
    QString nombre = nombreEdit->text().trimmed();
    int propietarioId = propCombo->currentData().toInt();
    int ubicacionId = ubiCombo->currentData().toInt();
    QVector<int> tipoIds = checkedTipoIdsFromList(tiposList);
    QJsonObject g = gatherPiezaParamValues(paramWidgets);
    for (auto it = g.begin(); it != g.end(); ++it)
        workingParams.insert(it.key(), it.value());
    pruneWorkingToChecked(&workingParams, tipoIds);
    m_overlay->showOverlay();
    m_piezaService->create(
        codigoEdit->text().trimmed(),
        nombre,
        notasEdit->text().trimmed(),
        fechaCheck->isChecked() ? fechaEdit->date() : QDate(),
        precioSpin->value(),
        propietarioId,
        ubicacionId,
        tipoIds,
        imagenesBase64,
        piezaParamsToJsonString(workingParams));
    });
}

void PiezasPage::onEdit(int piezaId)
{
    if (!m_supabase) return;
    m_overlay->showOverlay();
    connect(m_piezaService, &PiezaService::piezaForEditReady, this, [this, piezaId](const PiezaCardData& card) {
        m_overlay->hideOverlay();
        if (card.pieza.id == 0) {
            QMessageBox::warning(this, tr("Piezas"), tr("Pieza no encontrada."));
            return;
        }
        if (m_lastPropietarios.isEmpty()) {
            QMessageBox::warning(this, tr("Piezas"), tr("No hay propietarios. A\u00f1ade al menos uno."));
            return;
        }

        withTiposRefreshed([this, piezaId, card]() {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Editar pieza"));
        dlg.setMinimumSize(780, 520);

        auto* mainLay = new QVBoxLayout(&dlg);
        auto* hRow = new QHBoxLayout();

        auto* leftCol = new QWidget(&dlg);
        leftCol->setMinimumWidth(280);
        auto* leftLay = new QVBoxLayout(leftCol);
        leftLay->setContentsMargins(0, 0, 10, 0);

        auto* tiposGroup = new QGroupBox(tr("Tipos"), &dlg);
        auto* tiposLayout = new QVBoxLayout(tiposGroup);
        auto* tiposList = new QListWidget(&dlg);
        tiposList->setMinimumHeight(0);
        tiposList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tiposList->setStyleSheet(QStringLiteral(
            "QListWidget::indicator { width: 18px; height: 18px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
            "QListWidget::indicator:checked { background-color: #27ae60; border-color: #1e8449; image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDE4IDE4Ij48cGF0aCBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIiBkPSJNMyA5bDQgNCA4LTEyIi8+PC9zdmc+); }"));
        for (const auto& t : m_lastTipos) {
            auto* item = new QListWidgetItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre);
            item->setData(Qt::UserRole, t.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(card.tipoIds.contains(t.id) ? Qt::Checked : Qt::Unchecked);
            tiposList->addItem(item);
        }
        tiposLayout->addWidget(tiposList, 1);
        leftLay->addWidget(tiposGroup, 1);

        auto* paramsGroup = new QGroupBox(tr("Par\u00e1metros por tipo"), &dlg);
        auto* paramsGroupLay = new QVBoxLayout(paramsGroup);
        auto* paramsInner = new QWidget(paramsGroup);
        auto* paramsInnerLay = new QVBoxLayout(paramsInner);
        paramsInnerLay->setContentsMargins(4, 4, 4, 4);
        paramsGroupLay->addWidget(paramsInner);
        leftLay->addWidget(paramsGroup, 0);

        QJsonObject workingParams = workingParamsFromPieza(card.pieza);
        QHash<int, QMap<QString, QWidget*>> paramWidgets;
        auto refreshParamPanel = [paramsInnerLay, tiposList, this, &workingParams, &paramWidgets]() {
            QJsonObject g = gatherPiezaParamValues(paramWidgets);
            for (auto it = g.begin(); it != g.end(); ++it)
                workingParams.insert(it.key(), it.value());
            QVector<int> ids = checkedTipoIdsFromList(tiposList);
            pruneWorkingToChecked(&workingParams, ids);
            rebuildPiezaTipoParamsPanel(this, paramsInnerLay, ids, m_lastTipos, workingParams, &paramWidgets);
        };
        QObject::connect(tiposList, &QListWidget::itemChanged, &dlg,
            [refreshParamPanel](QListWidgetItem*) { refreshParamPanel(); });
        refreshParamPanel();

        hRow->addWidget(leftCol, 0);

        auto* rightCol = new QWidget(&dlg);
        auto* rightOuter = new QVBoxLayout(rightCol);
        auto* rightForm = new QFormLayout();

        auto* codigoEdit = new QLineEdit(&dlg);
        codigoEdit->setPlaceholderText(tr("C\u00f3digo opcional"));
        codigoEdit->setText(card.pieza.codigo);
        rightForm->addRow(tr("C\u00f3digo:"), codigoEdit);

        auto* nombreEdit = new QLineEdit(&dlg);
        nombreEdit->setPlaceholderText(tr("Nombre de la pieza"));
        nombreEdit->setText(card.pieza.nombre);
        rightForm->addRow(tr("Nombre:"), nombreEdit);

        auto* notasEdit = new QLineEdit(&dlg);
        notasEdit->setPlaceholderText(tr("Notas opcionales"));
        notasEdit->setText(card.pieza.notas);
        rightForm->addRow(tr("Notas:"), notasEdit);

        auto* fechaCheck = new QCheckBox(tr("Usar fecha"), &dlg);
        fechaCheck->setChecked(card.pieza.fecha.isValid());
        fechaCheck->setStyleSheet(QStringLiteral(
            "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
            "QCheckBox::indicator:checked { background-color: #409eff; border-color: #2b7cd3; }"));
        auto* fechaEdit = new QDateEdit(&dlg);
        fechaEdit->setCalendarPopup(true);
        fechaEdit->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
        fechaEdit->setDate(card.pieza.fecha.isValid() ? card.pieza.fecha : QDate::currentDate());
        fechaEdit->setEnabled(card.pieza.fecha.isValid());
        auto* fechaRow = new QHBoxLayout();
        fechaRow->addWidget(fechaCheck);
        fechaRow->addWidget(fechaEdit);
        rightForm->addRow(tr("Fecha:"), fechaRow);

        QObject::connect(fechaCheck, &QCheckBox::toggled, &dlg, [fechaEdit](bool on) {
            fechaEdit->setEnabled(on);
        });

        auto* precioSpin = new QDoubleSpinBox(&dlg);
        precioSpin->setRange(0, 999999.99);
        precioSpin->setDecimals(2);
        precioSpin->setValue(card.pieza.precio);
        rightForm->addRow(tr("Precio:"), precioSpin);

        auto* propCombo = new QComboBox(&dlg);
        for (const auto& p : m_lastPropietarios)
            propCombo->addItem(p.nombre, p.id);
        int propIdx = propCombo->findData(card.pieza.propietarioId);
        if (propIdx >= 0) propCombo->setCurrentIndex(propIdx);
        rightForm->addRow(tr("Propietario:"), propCombo);

        auto* ubiCombo = new QComboBox(&dlg);
        ubiCombo->addItem(tr("Sin ubicaci\u00f3n"), 0);
        for (const auto& u : m_lastUbicaciones)
            ubiCombo->addItem(u.nombre, u.id);
        int ubiIdx = ubiCombo->findData(card.pieza.ubicacionId);
        if (ubiIdx >= 0) ubiCombo->setCurrentIndex(ubiIdx);
        rightForm->addRow(tr("Ubicaci\u00f3n:"), ubiCombo);

        rightOuter->addLayout(rightForm);

        auto* imgsGroup = new QGroupBox(tr("Im\u00e1genes"), &dlg);
        auto* imgsLayout = new QVBoxLayout(imgsGroup);
        auto* imgsList = new QListWidget(&dlg);
        imgsList->setMinimumHeight(120);
        imgsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        imgsList->setDragDropMode(QAbstractItemView::InternalMove);
        imgsList->setDefaultDropAction(Qt::MoveAction);
        imgsLayout->addWidget(imgsList);
        for (int i = 0; i < card.imagenesBase64.size(); ++i) {
            auto* item = new QListWidgetItem(tr("Imagen %1").arg(i + 1));
            item->setData(Qt::UserRole, card.imagenesBase64[i]);
            imgsList->addItem(item);
        }
        auto* addImgBtn = new QPushButton(tr("A\u00f1adir imagen..."), &dlg);
        auto* removeImgBtn = new QPushButton(tr("Quitar imagen"), &dlg);
        auto* imgsBtnRow = new QHBoxLayout();
        imgsBtnRow->addWidget(addImgBtn);
        imgsBtnRow->addWidget(removeImgBtn);
        imgsBtnRow->addStretch(1);
        imgsLayout->addLayout(imgsBtnRow);
        rightOuter->addWidget(imgsGroup, 1);

        hRow->addWidget(rightCol, 1);
        mainLay->addLayout(hRow, 1);

        connect(addImgBtn, &QPushButton::clicked, &dlg, [&dlg, imgsList]() {
            QString path = QFileDialog::getOpenFileName(&dlg, tr("Seleccionar imagen"), QString(),
                tr("Im\u00e1genes (*.png *.jpg *.jpeg *.bmp *.gif)"));
            if (path.isEmpty()) return;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return;
            QByteArray ba = f.readAll().toBase64();
            f.close();
            if (ba.size() > 10 * 1024 * 1024) {
                QMessageBox::warning(&dlg, tr("Imagen"), tr("La imagen es demasiado grande (m\u00e1x. 10 MB)."));
                return;
            }
            auto* item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, QString::fromUtf8(ba));
            imgsList->addItem(item);
        });
        connect(removeImgBtn, &QPushButton::clicked, &dlg, [imgsList]() {
            int row = imgsList->currentRow();
            if (row >= 0) delete imgsList->takeItem(row);
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        mainLay->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, [nombreEdit, &dlg]() {
            if (nombreEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(&dlg, tr("Piezas"), tr("El nombre no puede estar vac\u00edo."));
                return;
            }
            dlg.accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return;

        QVector<QString> imagenesBase64;
        for (int i = 0; i < imgsList->count(); ++i)
            imagenesBase64.append(imgsList->item(i)->data(Qt::UserRole).toString());
        QString nombre = nombreEdit->text().trimmed();
        int propietarioId = propCombo->currentData().toInt();
        int ubicacionId = ubiCombo->currentData().toInt();
        QVector<int> tipoIds = checkedTipoIdsFromList(tiposList);
        QJsonObject g = gatherPiezaParamValues(paramWidgets);
        for (auto it = g.begin(); it != g.end(); ++it)
            workingParams.insert(it.key(), it.value());
        pruneWorkingToChecked(&workingParams, tipoIds);
        m_overlay->showOverlay();
        m_piezaService->update(piezaId, codigoEdit->text().trimmed(), nombre, notasEdit->text().trimmed(),
            fechaCheck->isChecked() ? fechaEdit->date() : QDate(), precioSpin->value(), propietarioId, ubicacionId,
            tipoIds, imagenesBase64, piezaParamsToJsonString(workingParams));
        });
    }, Qt::SingleShotConnection);
    m_piezaService->fetchPiezaForEdit(piezaId);
}

void PiezasPage::onDuplicate(int piezaId)
{
    if (!m_supabase) return;
    m_overlay->showOverlay();
    connect(m_piezaService, &PiezaService::piezaForEditReady, this, [this](const PiezaCardData& card) {
        m_overlay->hideOverlay();
        if (card.pieza.id == 0) {
            QMessageBox::warning(this, tr("Piezas"), tr("Pieza no encontrada."));
            return;
        }
        if (m_lastPropietarios.isEmpty()) {
            QMessageBox::warning(this, tr("Piezas"), tr("No hay propietarios. A\u00f1ade al menos uno."));
            return;
        }

        withTiposRefreshed([this, card]() {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Duplicar pieza"));
        dlg.setMinimumSize(780, 520);

        auto* mainLay = new QVBoxLayout(&dlg);
        auto* hRow = new QHBoxLayout();

        auto* leftCol = new QWidget(&dlg);
        leftCol->setMinimumWidth(280);
        auto* leftLay = new QVBoxLayout(leftCol);
        leftLay->setContentsMargins(0, 0, 10, 0);

        auto* tiposGroup = new QGroupBox(tr("Tipos"), &dlg);
        auto* tiposLayout = new QVBoxLayout(tiposGroup);
        auto* tiposList = new QListWidget(&dlg);
        tiposList->setMinimumHeight(0);
        tiposList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tiposList->setStyleSheet(QStringLiteral(
            "QListWidget::indicator { width: 18px; height: 18px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
            "QListWidget::indicator:checked { background-color: #27ae60; border-color: #1e8449; image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxOCIgaGVpZ2h0PSIxOCIgdmlld0JveD0iMCAwIDE4IDE4Ij48cGF0aCBmaWxsPSJub25lIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIiBkPSJNMyA5bDQgNCA4LTEyIi8+PC9zdmc+); }"));
        for (const auto& t : m_lastTipos) {
            auto* item = new QListWidgetItem(t.nombre.isEmpty() ? QString::number(t.id) : t.nombre);
            item->setData(Qt::UserRole, t.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(card.tipoIds.contains(t.id) ? Qt::Checked : Qt::Unchecked);
            tiposList->addItem(item);
        }
        tiposLayout->addWidget(tiposList, 1);
        leftLay->addWidget(tiposGroup, 1);

        auto* paramsGroup = new QGroupBox(tr("Par\u00e1metros por tipo"), &dlg);
        auto* paramsGroupLay = new QVBoxLayout(paramsGroup);
        auto* paramsInner = new QWidget(paramsGroup);
        auto* paramsInnerLay = new QVBoxLayout(paramsInner);
        paramsInnerLay->setContentsMargins(4, 4, 4, 4);
        paramsGroupLay->addWidget(paramsInner);
        leftLay->addWidget(paramsGroup, 0);

        QJsonObject workingParams = workingParamsFromPieza(card.pieza);
        QHash<int, QMap<QString, QWidget*>> paramWidgets;
        auto refreshParamPanel = [paramsInnerLay, tiposList, this, &workingParams, &paramWidgets]() {
            QJsonObject g = gatherPiezaParamValues(paramWidgets);
            for (auto it = g.begin(); it != g.end(); ++it)
                workingParams.insert(it.key(), it.value());
            QVector<int> ids = checkedTipoIdsFromList(tiposList);
            pruneWorkingToChecked(&workingParams, ids);
            rebuildPiezaTipoParamsPanel(this, paramsInnerLay, ids, m_lastTipos, workingParams, &paramWidgets);
        };
        QObject::connect(tiposList, &QListWidget::itemChanged, &dlg,
            [refreshParamPanel](QListWidgetItem*) { refreshParamPanel(); });
        refreshParamPanel();

        hRow->addWidget(leftCol, 0);

        auto* rightCol = new QWidget(&dlg);
        auto* rightOuter = new QVBoxLayout(rightCol);
        auto* rightForm = new QFormLayout();

        auto* codigoEdit = new QLineEdit(&dlg);
        codigoEdit->setPlaceholderText(tr("C\u00f3digo opcional"));
        codigoEdit->setText(card.pieza.codigo);
        rightForm->addRow(tr("C\u00f3digo:"), codigoEdit);

        auto* nombreEdit = new QLineEdit(&dlg);
        nombreEdit->setPlaceholderText(tr("Nombre de la pieza"));
        nombreEdit->setText(card.pieza.nombre);
        rightForm->addRow(tr("Nombre:"), nombreEdit);

        auto* notasEdit = new QLineEdit(&dlg);
        notasEdit->setPlaceholderText(tr("Notas opcionales"));
        notasEdit->setText(card.pieza.notas);
        rightForm->addRow(tr("Notas:"), notasEdit);

        auto* fechaCheck = new QCheckBox(tr("Usar fecha"), &dlg);
        fechaCheck->setChecked(card.pieza.fecha.isValid());
        fechaCheck->setStyleSheet(QStringLiteral(
            "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #bdc3c7; border-radius: 3px; background-color: #ffffff; }"
            "QCheckBox::indicator:checked { background-color: #409eff; border-color: #2b7cd3; }"));
        auto* fechaEdit = new QDateEdit(&dlg);
        fechaEdit->setCalendarPopup(true);
        fechaEdit->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
        fechaEdit->setDate(card.pieza.fecha.isValid() ? card.pieza.fecha : QDate::currentDate());
        fechaEdit->setEnabled(card.pieza.fecha.isValid());
        auto* fechaRow = new QHBoxLayout();
        fechaRow->addWidget(fechaCheck);
        fechaRow->addWidget(fechaEdit);
        rightForm->addRow(tr("Fecha:"), fechaRow);

        QObject::connect(fechaCheck, &QCheckBox::toggled, &dlg, [fechaEdit](bool on) {
            fechaEdit->setEnabled(on);
        });

        auto* precioSpin = new QDoubleSpinBox(&dlg);
        precioSpin->setRange(0, 999999.99);
        precioSpin->setDecimals(2);
        precioSpin->setValue(card.pieza.precio);
        rightForm->addRow(tr("Precio:"), precioSpin);

        auto* propCombo = new QComboBox(&dlg);
        for (const auto& p : m_lastPropietarios)
            propCombo->addItem(p.nombre, p.id);
        int propIdx = propCombo->findData(card.pieza.propietarioId);
        if (propIdx >= 0) propCombo->setCurrentIndex(propIdx);
        rightForm->addRow(tr("Propietario:"), propCombo);

        auto* ubiCombo = new QComboBox(&dlg);
        ubiCombo->addItem(tr("Sin ubicaci\u00f3n"), 0);
        for (const auto& u : m_lastUbicaciones)
            ubiCombo->addItem(u.nombre, u.id);
        int ubiIdx = ubiCombo->findData(card.pieza.ubicacionId);
        if (ubiIdx >= 0) ubiCombo->setCurrentIndex(ubiIdx);
        rightForm->addRow(tr("Ubicaci\u00f3n:"), ubiCombo);

        rightOuter->addLayout(rightForm);

        auto* imgsGroup = new QGroupBox(tr("Im\u00e1genes"), &dlg);
        auto* imgsLayout = new QVBoxLayout(imgsGroup);
        auto* imgsList = new QListWidget(&dlg);
        imgsList->setMinimumHeight(120);
        imgsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        imgsList->setDragDropMode(QAbstractItemView::InternalMove);
        imgsList->setDefaultDropAction(Qt::MoveAction);
        imgsLayout->addWidget(imgsList);
        for (int i = 0; i < card.imagenesBase64.size(); ++i) {
            auto* item = new QListWidgetItem(tr("Imagen %1").arg(i + 1));
            item->setData(Qt::UserRole, card.imagenesBase64[i]);
            imgsList->addItem(item);
        }
        auto* addImgBtn = new QPushButton(tr("A\u00f1adir imagen..."), &dlg);
        auto* removeImgBtn = new QPushButton(tr("Quitar imagen"), &dlg);
        auto* imgsBtnRow = new QHBoxLayout();
        imgsBtnRow->addWidget(addImgBtn);
        imgsBtnRow->addWidget(removeImgBtn);
        imgsBtnRow->addStretch(1);
        imgsLayout->addLayout(imgsBtnRow);
        rightOuter->addWidget(imgsGroup, 1);

        hRow->addWidget(rightCol, 1);
        mainLay->addLayout(hRow, 1);

        connect(addImgBtn, &QPushButton::clicked, &dlg, [&dlg, imgsList]() {
            QString path = QFileDialog::getOpenFileName(&dlg, tr("Seleccionar imagen"), QString(),
                tr("Im\u00e1genes (*.png *.jpg *.jpeg *.bmp *.gif)"));
            if (path.isEmpty()) return;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return;
            QByteArray ba = f.readAll().toBase64();
            f.close();
            if (ba.size() > 10 * 1024 * 1024) {
                QMessageBox::warning(&dlg, tr("Imagen"), tr("La imagen es demasiado grande (m\u00e1x. 10 MB)."));
                return;
            }
            auto* item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, QString::fromUtf8(ba));
            imgsList->addItem(item);
        });
        connect(removeImgBtn, &QPushButton::clicked, &dlg, [imgsList]() {
            int row = imgsList->currentRow();
            if (row >= 0) delete imgsList->takeItem(row);
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        mainLay->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, [nombreEdit, &dlg]() {
            if (nombreEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(&dlg, tr("Piezas"), tr("El nombre no puede estar vac\u00edo."));
                return;
            }
            dlg.accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() != QDialog::Accepted) return;

        QVector<QString> imagenesBase64;
        for (int i = 0; i < imgsList->count(); ++i)
            imagenesBase64.append(imgsList->item(i)->data(Qt::UserRole).toString());
        QString nombre = nombreEdit->text().trimmed();
        int propietarioId = propCombo->currentData().toInt();
        int ubicacionId = ubiCombo->currentData().toInt();
        QVector<int> tipoIds = checkedTipoIdsFromList(tiposList);
        QJsonObject g = gatherPiezaParamValues(paramWidgets);
        for (auto it = g.begin(); it != g.end(); ++it)
            workingParams.insert(it.key(), it.value());
        pruneWorkingToChecked(&workingParams, tipoIds);
        m_overlay->showOverlay();
        m_piezaService->create(
            codigoEdit->text().trimmed(),
            nombre,
            notasEdit->text().trimmed(),
            fechaCheck->isChecked() ? fechaEdit->date() : QDate(),
            precioSpin->value(),
            propietarioId,
            ubicacionId,
            tipoIds,
            imagenesBase64,
            piezaParamsToJsonString(workingParams));
        });
    }, Qt::SingleShotConnection);
    m_piezaService->fetchPiezaForEdit(piezaId);
}

void PiezasPage::onDelete(int piezaId)
{
    if (!m_supabase) return;
    auto ret = QMessageBox::question(this, tr("Eliminar pieza"),
        tr("\u00bfEliminar esta pieza?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_overlay->showOverlay();
    m_piezaService->remove(piezaId);
}
