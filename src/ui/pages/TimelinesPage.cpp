#include "TimelinesPage.h"
#include "core/SupabaseClient.h"
#include "core/TimelineService.h"
#include "core/UsadaService.h"
#include "core/PiezaService.h"
#include "core/PiezaCardData.h"
#include "core/TipoService.h"
#include "models/Timeline.h"
#include "models/UsadaCanvasPlacement.h"
#include "models/Tipo.h"
#include "ui/LoadingOverlay.h"
#include "ui/TimelineTopBoardView.h"
#include <QApplication>
#include <QObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QSplitter>
#include <QFrame>
#include <QButtonGroup>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSet>
#include <QVector>
#include <QComboBox>
#include <QHash>
#include <QSignalBlocker>
#include <QVariantList>
#include <QVariantMap>
#include <QDoubleSpinBox>
#include <QLocale>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QQueue>
#include <QTimer>
#include <QScrollArea>
#include <QPixmap>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QtMath>
#include <algorithm>
#include <chrono>
#include <functional>

namespace {

constexpr int kPiezaListIdRole = Qt::UserRole + 64;
constexpr double kDefaultLongitudCm = 300.0;
constexpr double kDefaultAnchuraCm = 200.0;
constexpr double kBoardSpinMinCm = 1.0;
constexpr double kBoardSpinMaxCm = 10000.0;

static const QString kPieceListStyle = QStringLiteral(
    "QListWidget { background: transparent; border: none; outline: none; color: #334155; "
    "font-size: 12px; cursor: PointingHandCursor; }"
    "QListWidget::item { padding: 2px 3px; border: none; min-height: 16px; }"
    "QListWidget::item:alternate { background: rgba(0,0,0,0.04); }"
    "QListWidget::item:hover { background-color: #cfd8e4; }");

static void addPropsFormRow(QFormLayout* form, const QString& label, const QString& value)
{
    if (!form)
        return;
    auto* v = new QLabel(value);
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setStyleSheet(QStringLiteral("color: #1e293b; font-size: 13px;"));
    form->addRow(label + QStringLiteral(":"), v);
}

static QString jsonParamValueToDisplay(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool() ? QStringLiteral("s\u00ed") : QStringLiteral("no");
    if (v.isDouble()) {
        const double d = v.toDouble();
        const double r = qRound64(d);
        if (qFuzzyCompare(d + 1.0, r + 1.0))
            return QString::number(static_cast<qint64>(r));
        return QLocale().toString(d, 'g', 10);
    }
    if (v.isString())
        return v.toString();
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return QStringLiteral("\u2014");
}

static QString tipoNombreFromPiezaCard(const PiezaCardData& card, int tipoId)
{
    for (int i = 0; i < card.tipoIds.size() && i < card.tipos.size(); ++i) {
        if (card.tipoIds.at(i) == tipoId) {
            const QString n = card.tipos.at(i).trimmed();
            if (!n.isEmpty())
                return n;
            break;
        }
    }
    return QStringLiteral("#%1").arg(tipoId);
}

static void addPiezaParamsToPropsForm(QFormLayout* form, const PiezaCardData& card, QWidget* trContext)
{
    if (!form || !trContext)
        return;
    const QString raw = card.pieza.paramsJson.trimmed();
    if (raw.isEmpty() || raw == QLatin1String("{}"))
        return;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const QJsonObject root = doc.object();
    if (root.isEmpty())
        return;

    QStringList tipoKeys;
    tipoKeys.reserve(root.size());
    for (auto it = root.begin(); it != root.end(); ++it)
        tipoKeys.append(it.key());
    std::sort(tipoKeys.begin(), tipoKeys.end(), [](const QString& a, const QString& b) {
        bool okA = false;
        bool okB = false;
        const int ia = a.toInt(&okA);
        const int ib = b.toInt(&okB);
        if (okA && okB)
            return ia < ib;
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });

    for (const QString& key : tipoKeys) {
        const QJsonValue blockV = root.value(key);
        if (!blockV.isObject())
            continue;
        const QJsonObject vars = blockV.toObject();
        if (vars.isEmpty())
            continue;

        bool okId = false;
        const int tipoId = key.toInt(&okId);
        const QString tipoNombre =
            okId ? tipoNombreFromPiezaCard(card, tipoId) : key;

        QStringList varKeys;
        varKeys.reserve(vars.size());
        for (auto it = vars.begin(); it != vars.end(); ++it)
            varKeys.append(it.key());
        std::sort(varKeys.begin(), varKeys.end(), [](const QString& a, const QString& b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });

        QStringList parts;
        parts.reserve(varKeys.size());
        for (const QString& vk : varKeys) {
            parts.append(QStringLiteral("%1: %2").arg(vk, jsonParamValueToDisplay(vars.value(vk))));
        }
        const QString valueText = parts.join(QStringLiteral("  \u00B7  "));

        addPropsFormRow(form,
                        trContext->tr("Par\u00e1metros (%1)").arg(tipoNombre),
                        valueText);
    }
}

/** Lista de piezas con arrastre hacia el canvas (MIME application/x-belencreator-pieza-id). */
class PiezaDragListWidget : public QListWidget
{
public:
    explicit PiezaDragListWidget(QWidget* parent = nullptr)
        : QListWidget(parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        QListWidget::mousePressEvent(e);
        m_dragPiezaId = 0;
        if (e->button() != Qt::LeftButton)
            return;
        m_dragStart = e->pos();
        if (QListWidgetItem* it = itemAt(e->pos()))
            m_dragPiezaId = it->data(kPiezaListIdRole).toInt();
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!(e->buttons() & Qt::LeftButton) || m_dragPiezaId <= 0) {
            QListWidget::mouseMoveEvent(e);
            return;
        }
        if ((e->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
            QListWidget::mouseMoveEvent(e);
            return;
        }
        auto* md = new QMimeData;
        md->setData(QStringLiteral("application/x-belencreator-pieza-id"),
                    QByteArray::number(m_dragPiezaId));
        QDrag drag(this);
        drag.setMimeData(md);
        drag.exec(Qt::CopyAction);
        m_dragPiezaId = 0;
    }

private:
    QPoint m_dragStart;
    int m_dragPiezaId = 0;
};

/** Canvas inferior (placeholder). */
class TimelineCanvasWidget : public QFrame
{
public:
    explicit TimelineCanvasWidget(const QString& placeholderTitle, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("timelineCanvas"));
        setFrameShape(QFrame::StyledPanel);
        setFrameShadow(QFrame::Plain);
        setMinimumSize(160, 120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setStyleSheet(QStringLiteral(
            "QFrame#timelineCanvas { background-color: #f8f9fb; border: 1px solid #cfd8dc; "
            "border-radius: 2px; }"));
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        auto* hint = new QLabel(placeholderTitle, this);
        hint->setAlignment(Qt::AlignCenter);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #90a4ae; font-size: 12px; border: none; "
                                           "background: transparent;"));
        lay->addStretch(1);
        lay->addWidget(hint);
        lay->addStretch(1);
    }
};

static void styleSplitterHandle(QSplitter* splitter)
{
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background-color: #b0bec5; }"
        "QSplitter::handle:hover { background-color: #90a4ae; }"
        "QSplitter::handle:pressed { background-color: #78909c; }"));
    splitter->setHandleWidth(6);
    splitter->setChildrenCollapsible(false);
}

static void styleTimelineSplitters(QSplitter* horizontal, QSplitter* vertical)
{
    styleSplitterHandle(horizontal);
    styleSplitterHandle(vertical);
}

/** Cabecera del editor de un timeline concreto (sin Q_OBJECT). */
class TimelineEditorWidget : public QWidget
{
    enum class PlacementJobKind { DropOnBoard, CanvasFromDb };
    struct PlacementPreviewJob {
        PlacementJobKind kind = PlacementJobKind::DropOnBoard;
        int piezaId = 0;
        QPointF posCm;
        int usadaId = 0;
    };

public:
    explicit TimelineEditorWidget(UsadaService* usada, PiezaService* pieza, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_usada(usada)
        , m_pieza(pieza)
        , m_timelineId(0)
        , m_mainSplit(nullptr)
        , m_leftSplit(nullptr)
        , m_sideVertSplit(nullptr)
        , m_usedPiecesList(nullptr)
        , m_usedPiecesEmpty(nullptr)
        , m_unusedPiecesList(nullptr)
        , m_unusedPiecesEmpty(nullptr)
        , m_gotUsadas(false)
        , m_gotTodasPiezas(false)
        , m_pieceFilterSearch(nullptr)
        , m_pieceFilterTipoCombo(nullptr)
        , m_initialSplitGeomDone(false)
        , m_sideVertSplitInitialDone(false)
        , m_topBoard(nullptr)
        , m_canvasBottom(nullptr)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* topBlock = new QWidget(this);
        auto* topOuter = new QVBoxLayout(topBlock);
        topOuter->setContentsMargins(24, 24, 24, 12);
        topOuter->setSpacing(0);

        auto* topRow = new QHBoxLayout();
        topRow->setSpacing(16);
        topRow->setAlignment(Qt::AlignTop);

        m_heading = new QLabel(topBlock);
        m_heading->setTextFormat(Qt::RichText);
        m_heading->setStyleSheet(QStringLiteral("font-size: 20px; color: #2c3e50;"));
        m_heading->setWordWrap(true);
        m_heading->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        m_exportBtn = new QPushButton(tr("Exportar"), topBlock);
        m_exportBtn->setCursor(Qt::PointingHandCursor);
        m_exportBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #27ae60; color: white; border: none; "
            "padding: 10px 18px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #2ecc71; }"));

        m_backBtn = new QPushButton(tr("Volver al listado"), topBlock);
        m_backBtn->setCursor(Qt::PointingHandCursor);
        m_backBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #95a5a6; color: white; border: none; "
            "padding: 10px 18px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #7f8c8d; }"));

        topRow->addWidget(m_heading, 1, Qt::AlignTop);
        topRow->addWidget(m_exportBtn, 0, Qt::AlignTop);
        topRow->addWidget(m_backBtn, 0, Qt::AlignTop);
        topOuter->addLayout(topRow);
        root->addWidget(topBlock);

        m_mainSplit = new QSplitter(Qt::Horizontal, this);
        m_leftSplit = new QSplitter(Qt::Vertical, m_mainSplit);
        styleTimelineSplitters(m_mainSplit, m_leftSplit);

        m_topBoard = new TimelineTopBoardView(m_leftSplit);
        m_topBoard->setBoardCm(kDefaultLongitudCm, kDefaultAnchuraCm);
        m_topBoard->setCanvasPieceMovedHandler([this](int usadaId, QPointF c) {
            if (m_usada && usadaId > 0)
                m_usada->updateCanvasPlacementPosition(usadaId, c.x(), c.y());
        });
        m_topBoard->setCanvasPieceSelectionHandler([this](int piezaId, int usadaRowId) {
            onCanvasPieceSelected(piezaId, usadaRowId);
        });
        m_canvasBottom = new TimelineCanvasWidget(tr("Canvas inferior (dibujo)"), m_leftSplit);
        m_leftSplit->addWidget(m_topBoard);
        m_leftSplit->addWidget(m_canvasBottom);

        m_sidePanel = new QWidget(m_mainSplit);
        m_sidePanel->setObjectName(QStringLiteral("timelineEditorSidePanel"));
        m_sidePanel->setMinimumWidth(200);
        const QString sidePanelBg = QStringLiteral("#e8ecf1");
        m_sidePanel->setStyleSheet(QStringLiteral(
            "QWidget#timelineEditorSidePanel { background-color: %1; border-left: 1px solid #c5d0d9; }")
                                         .arg(sidePanelBg));

        auto* sideLay = new QVBoxLayout(m_sidePanel);
        sideLay->setContentsMargins(8, 8, 8, 8);
        sideLay->setSpacing(0);

        auto* topChrome = new QFrame(m_sidePanel);
        topChrome->setObjectName(QStringLiteral("timelineSideTopChrome"));
        topChrome->setFrameShape(QFrame::NoFrame);
        topChrome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        topChrome->setStyleSheet(QStringLiteral(
            "QFrame#timelineSideTopChrome { background-color: %1; border: none; }").arg(sidePanelBg));
        auto* topLay = new QVBoxLayout(topChrome);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->setSpacing(0);

        auto* piecesTitle = new QLabel(tr("PIEZAS"), topChrome);
        piecesTitle->setAlignment(Qt::AlignHCenter);
        piecesTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        piecesTitle->setStyleSheet(QStringLiteral(
            "color: #1e293b; font-size: 14px; font-weight: 700; margin: 0; padding: 0; "
            "background: transparent; border: none;"));
        topLay->addWidget(piecesTitle);

        auto* navRow = new QHBoxLayout();
        navRow->setSpacing(0);
        navRow->setContentsMargins(0, 0, 0, 0);

        auto* btnUsadas = new QPushButton(tr("Usadas"), topChrome);
        auto* btnNoUsadas = new QPushButton(tr("No usadas"), topChrome);
        for (auto* b : { btnUsadas, btnNoUsadas }) {
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumHeight(28);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        btnUsadas->setChecked(true);
        const QString navBtnBase = QStringLiteral(
            "QPushButton { padding: 2px 8px; font-size: 12px; font-weight: 600; "
            "border: 1px solid #c5ced9; background-color: #dde3ea; color: #475569; }"
            "QPushButton:checked { background-color: #3d5a80; color: #ffffff; border-color: #2f4766; }"
            "QPushButton:hover:!checked { background-color: #d0d8e2; border-color: #b8c4d4; color: #334155; }"
            "QPushButton:hover:checked { background-color: #4a6fa5; border-color: #3d5a80; }"
            "QPushButton:focus { outline: none; }");
        btnUsadas->setStyleSheet(navBtnBase
            + QStringLiteral("QPushButton { border-top-left-radius: 5px; border-bottom-left-radius: 5px; "
                             "border-top-right-radius: 0; border-bottom-right-radius: 0; border-right: none; }"));
        btnNoUsadas->setStyleSheet(navBtnBase
            + QStringLiteral("QPushButton { border-top-right-radius: 5px; border-bottom-right-radius: 5px; "
                             "border-top-left-radius: 0; border-bottom-left-radius: 0; }"));

        navRow->addWidget(btnUsadas);
        navRow->addWidget(btnNoUsadas);
        topLay->addLayout(navRow);

        topLay->addSpacing(6);
        auto* filterRow = new QHBoxLayout();
        filterRow->setSpacing(8);
        filterRow->setContentsMargins(0, 0, 0, 0);
        static const char* pieceFilterStyle =
            "QLineEdit, QComboBox { padding: 4px 8px; border: 1px solid #c5ced9; "
            "border-radius: 4px; background: #fff; font-size: 12px; min-height: 18px; }"
            "QLineEdit:focus, QComboBox:focus { border-color: #3d5a80; outline: none; }"
            "QComboBox::drop-down { border: none; padding-right: 4px; }"
            "QComboBox:hover { border-color: #b8c4d4; }";
        m_pieceFilterSearch = new QLineEdit(topChrome);
        m_pieceFilterSearch->setPlaceholderText(tr("Buscar pieza..."));
        m_pieceFilterSearch->setClearButtonEnabled(true);
        m_pieceFilterSearch->setStyleSheet(QString::fromUtf8(pieceFilterStyle));
        m_pieceFilterTipoCombo = new QComboBox(topChrome);
        m_pieceFilterTipoCombo->addItem(tr("Tipo: Todos"), 0);
        m_pieceFilterTipoCombo->setMinimumWidth(120);
        m_pieceFilterTipoCombo->setStyleSheet(QString::fromUtf8(pieceFilterStyle));
        filterRow->addWidget(m_pieceFilterSearch, 1);
        filterRow->addWidget(m_pieceFilterTipoCombo, 0);
        topLay->addLayout(filterRow);

        connect(m_pieceFilterSearch, &QLineEdit::textChanged, this, [this](const QString&) {
            rebuildUsedPiecesList();
            rebuildNoUsadasList();
        });
        connect(m_pieceFilterTipoCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            rebuildUsedPiecesList();
            rebuildNoUsadasList();
        });

        auto* navGroup = new QButtonGroup(m_sidePanel);
        navGroup->setExclusive(true);
        navGroup->addButton(btnUsadas, 0);
        navGroup->addButton(btnNoUsadas, 1);

        auto* contentCard = new QFrame(m_sidePanel);
        contentCard->setObjectName(QStringLiteral("timelineSideContentCard"));
        contentCard->setFrameShape(QFrame::NoFrame);
        contentCard->setStyleSheet(QStringLiteral(
            "QFrame#timelineSideContentCard { background-color: %1; border: none; }").arg(sidePanelBg));
        auto* contentLay = new QVBoxLayout(contentCard);
        contentLay->setContentsMargins(0, 0, 0, 0);
        contentLay->setSpacing(0);

        auto* sideStack = new QStackedWidget(contentCard);
        sideStack->setObjectName(QStringLiteral("timelineSideStack"));
        sideStack->setStyleSheet(QStringLiteral(
            "QStackedWidget#timelineSideStack { background: transparent; border: none; }"));
        sideStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        const int contentPad = 8;
        auto* pageUsadas = new QWidget(sideStack);
        auto* layUsadas = new QVBoxLayout(pageUsadas);
        layUsadas->setContentsMargins(contentPad, contentPad, contentPad, contentPad);
        layUsadas->setSpacing(6);
        m_usedPiecesEmpty = new QLabel(tr("Cargando\u2026"), pageUsadas);
        m_usedPiecesEmpty->setWordWrap(true);
        m_usedPiecesEmpty->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        m_usedPiecesEmpty->setStyleSheet(QStringLiteral(
            "color: #64748b; font-size: 13px; background: transparent; border: none;"));
        m_usedPiecesList = new QListWidget(pageUsadas);
        m_usedPiecesList->setFrameShape(QFrame::NoFrame);
        m_usedPiecesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_usedPiecesList->setSelectionMode(QAbstractItemView::NoSelection);
        m_usedPiecesList->setFocusPolicy(Qt::NoFocus);
        m_usedPiecesList->setAlternatingRowColors(true);
        m_usedPiecesList->setStyleSheet(kPieceListStyle);
        m_usedPiecesList->setCursor(Qt::PointingHandCursor);
        m_usedPiecesList->viewport()->setCursor(Qt::PointingHandCursor);
        m_usedPiecesList->hide();
        layUsadas->addWidget(m_usedPiecesEmpty);
        layUsadas->addWidget(m_usedPiecesList, 1);

        auto* pageNoUsadas = new QWidget(sideStack);
        auto* layNoUsadas = new QVBoxLayout(pageNoUsadas);
        layNoUsadas->setContentsMargins(contentPad, contentPad, contentPad, contentPad);
        layNoUsadas->setSpacing(6);
        m_unusedPiecesEmpty = new QLabel(tr("Cargando\u2026"), pageNoUsadas);
        m_unusedPiecesEmpty->setWordWrap(true);
        m_unusedPiecesEmpty->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        m_unusedPiecesEmpty->setStyleSheet(QStringLiteral(
            "color: #64748b; font-size: 13px; background: transparent; border: none;"));
        m_unusedPiecesList = new PiezaDragListWidget(pageNoUsadas);
        m_unusedPiecesList->setFrameShape(QFrame::NoFrame);
        m_unusedPiecesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_unusedPiecesList->setSelectionMode(QAbstractItemView::NoSelection);
        m_unusedPiecesList->setFocusPolicy(Qt::NoFocus);
        m_unusedPiecesList->setAlternatingRowColors(true);
        m_unusedPiecesList->setStyleSheet(kPieceListStyle);
        m_unusedPiecesList->setCursor(Qt::PointingHandCursor);
        m_unusedPiecesList->viewport()->setCursor(Qt::PointingHandCursor);
        m_unusedPiecesList->hide();
        layNoUsadas->addWidget(m_unusedPiecesEmpty);
        layNoUsadas->addWidget(m_unusedPiecesList, 1);

        sideStack->addWidget(pageUsadas);
        sideStack->addWidget(pageNoUsadas);
        contentLay->addWidget(sideStack);

        connect(navGroup, &QButtonGroup::idClicked, sideStack, &QStackedWidget::setCurrentIndex);

        auto* piecesPanel = new QWidget(m_sidePanel);
        piecesPanel->setStyleSheet(QStringLiteral("background-color: %1;").arg(sidePanelBg));
        piecesPanel->setMinimumHeight(100);
        auto* piecesLay = new QVBoxLayout(piecesPanel);
        piecesLay->setContentsMargins(0, 0, 0, 0);
        piecesLay->setSpacing(0);
        piecesLay->addWidget(topChrome, 0);
        piecesLay->addWidget(contentCard, 1);

        auto* propsPanel = new QWidget(m_sidePanel);
        propsPanel->setStyleSheet(QStringLiteral("background-color: %1;").arg(sidePanelBg));
        propsPanel->setMinimumHeight(120);
        auto* propsLay = new QVBoxLayout(propsPanel);
        propsLay->setContentsMargins(0, 8, 0, 0);
        propsLay->setSpacing(6);
        auto* propsTitle = new QLabel(tr("PROPIEDADES"), propsPanel);
        propsTitle->setAlignment(Qt::AlignHCenter);
        propsTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        propsTitle->setStyleSheet(QStringLiteral(
            "color: #1e293b; font-size: 14px; font-weight: 700; margin: 0; padding: 0; "
            "background: transparent; border: none;"));
        propsLay->addWidget(propsTitle);

        m_propsScroll = new QScrollArea(propsPanel);
        m_propsScroll->setWidgetResizable(true);
        m_propsScroll->setFrameShape(QFrame::NoFrame);
        m_propsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_propsScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
        m_propsInner = new QWidget();
        m_propsInner->setStyleSheet(QStringLiteral("background: transparent;"));
        auto* innerLay = new QVBoxLayout(m_propsInner);
        innerLay->setContentsMargins(8, 0, 8, 0);
        innerLay->setSpacing(8);

        m_propsPlaceholder = new QLabel(tr("Seleccione una pieza en el tablero superior."), m_propsInner);
        m_propsPlaceholder->setWordWrap(true);
        m_propsPlaceholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_propsPlaceholder->setStyleSheet(QStringLiteral(
            "color: #64748b; font-size: 13px; background: transparent; border: none;"));
        innerLay->addWidget(m_propsPlaceholder);

        m_propsImageLabel = new QLabel(m_propsInner);
        m_propsImageLabel->setAlignment(Qt::AlignCenter);
        m_propsImageLabel->setMinimumHeight(1);
        m_propsImageLabel->hide();
        innerLay->addWidget(m_propsImageLabel);

        m_propsFormHost = new QWidget(m_propsInner);
        m_propsForm = new QFormLayout(m_propsFormHost);
        m_propsForm->setSpacing(6);
        m_propsForm->setContentsMargins(0, 0, 0, 0);
        m_propsFormHost->hide();
        innerLay->addWidget(m_propsFormHost);
        innerLay->addStretch(1);

        m_propsScroll->setWidget(m_propsInner);
        propsLay->addWidget(m_propsScroll, 1);

        m_propsRemoveFromCanvasBtn = new QPushButton(tr("Quitar del tablero"), propsPanel);
        m_propsRemoveFromCanvasBtn->setCursor(Qt::PointingHandCursor);
        m_propsRemoveFromCanvasBtn->setEnabled(false);
        m_propsRemoveFromCanvasBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #c0392b; color: white; border: none; "
            "padding: 8px 10px; border-radius: 4px; font-size: 13px; }"
            "QPushButton:hover:enabled { background-color: #e74c3c; }"
            "QPushButton:disabled { background-color: #b0b8c4; color: #e8eaed; }"));
        connect(m_propsRemoveFromCanvasBtn, &QPushButton::clicked, this, [this]() {
            if (m_selectedCanvasUsadaId <= 0 || !m_usada)
                return;
            const auto ret =
                QMessageBox::question(this, tr("Quitar del tablero"),
                                      tr("\u00bfEliminar esta pieza del tablero? "
                                         "La posici\u00f3n guardada se perder\u00e1."),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes)
                return;
            m_usada->deleteCanvasPlacement(m_selectedCanvasUsadaId);
        });

        propsLay->addWidget(m_propsRemoveFromCanvasBtn, 0);

        m_sideVertSplit = new QSplitter(Qt::Vertical, m_sidePanel);
        m_sideVertSplit->setObjectName(QStringLiteral("timelineSideVertSplit"));
        styleSplitterHandle(m_sideVertSplit);
        m_sideVertSplit->setStretchFactor(0, 2);
        m_sideVertSplit->setStretchFactor(1, 1);
        m_sideVertSplit->addWidget(piecesPanel);
        m_sideVertSplit->addWidget(propsPanel);

        sideLay->addWidget(m_sideVertSplit, 1);

        m_mainSplit->addWidget(m_leftSplit);
        m_mainSplit->addWidget(m_sidePanel);
        m_mainSplit->setStretchFactor(0, 1);
        m_mainSplit->setStretchFactor(1, 0);
        root->addWidget(m_mainSplit, 1);

        connect(m_backBtn, &QPushButton::clicked, this, [this]() {
            if (m_onBack) m_onBack();
        });
        connect(m_exportBtn, &QPushButton::clicked, this, [this]() {
            if (m_onExport) m_onExport();
        });

        if (m_usada) {
            connect(m_usada, &UsadaService::canvasPlacementInsertFinished, this,
                    [this](bool ok, const QString& err, int newId) {
                        finishPendingCanvasInsert(ok, err, newId);
                    });
            connect(m_usada, &UsadaService::canvasPlacementDeleteFinished, this,
                    [this](bool ok, const QString& err, int usadaId) {
                        if (!ok) {
                            QMessageBox::warning(this, tr("Timelines"),
                                                 err.isEmpty() ? tr("No se pudo quitar la pieza del tablero.")
                                                               : err);
                            return;
                        }
                        if (m_topBoard)
                            m_topBoard->removeCanvasPieceByUsadaId(usadaId);
                        if (m_timelineId > 0 && m_usada)
                            m_usada->fetchPiezasNombresPorTimeline(m_timelineId);
                    });
        }

        if (m_topBoard) {
            m_topBoard->setDropHandler([this](int piezaId, QPointF posCm) {
                enqueuePlacementPreview({PlacementJobKind::DropOnBoard, piezaId, posCm});
            });
        }
    }

    void enqueuePlacementPreview(PlacementPreviewJob job)
    {
        if (job.piezaId <= 0)
            return;
        m_previewJobs.enqueue(job);
        pumpPlacementPreviewIfIdle();
    }

    void pumpPlacementPreviewIfIdle()
    {
        if (m_previewInFlight || !m_pieza || m_previewJobs.isEmpty())
            return;
        m_previewInFlight = true;
        m_activePreviewPiezaId = m_previewJobs.head().piezaId;
        m_pieza->fetchPiezaPlacementPreview(m_activePreviewPiezaId);
    }

    void onPiezaPlacementPreviewFromService(int piezaId, const QString& b64, double largoCm, double anchoCm,
                                            bool hasLargo, bool hasAncho, bool /*tieneAcciones*/)
    {
        if (!m_previewInFlight || piezaId != m_activePreviewPiezaId || m_previewJobs.isEmpty())
            return;
        if (m_previewJobs.head().piezaId != piezaId)
            return;

        const PlacementPreviewJob job = m_previewJobs.dequeue();

        auto addImage = [this, b64, largoCm, anchoCm, hasLargo, hasAncho, job](const QPointF& pos,
                                                                               int usadaRowId) {
            if (!m_topBoard)
                return;
            const QString nom = m_nombrePorPiezaId.value(job.piezaId, QStringLiteral("#%1").arg(job.piezaId));
            m_topBoard->addPiezaImage(b64, largoCm, anchoCm, hasLargo, hasAncho, pos, usadaRowId, job.piezaId,
                                      nom);
        };

        const auto afterSync = [this]() {
            m_previewInFlight = false;
            m_activePreviewPiezaId = 0;
            pumpPlacementPreviewIfIdle();
        };

        if (job.kind == PlacementJobKind::CanvasFromDb) {
            addImage(job.posCm, job.usadaId);
            afterSync();
            return;
        }

        if (m_timelineId <= 0 || !m_usada) {
            addImage(job.posCm, 0);
            afterSync();
            return;
        }

        m_pendingCanvasInsert.active = true;
        m_pendingCanvasInsert.job = job;
        m_pendingCanvasInsert.b64 = b64;
        m_pendingCanvasInsert.largoCm = largoCm;
        m_pendingCanvasInsert.anchoCm = anchoCm;
        m_pendingCanvasInsert.hasLargo = hasLargo;
        m_pendingCanvasInsert.hasAncho = hasAncho;
        m_usada->insertCanvasPlacement(m_timelineId, piezaId, job.posCm.x(), job.posCm.y());
    }

    void finishPendingCanvasInsert(bool ok, const QString& err, int newUsadaId)
    {
        if (!m_pendingCanvasInsert.active)
            return;
        m_pendingCanvasInsert.active = false;
        const PlacementPreviewJob job = m_pendingCanvasInsert.job;
        const QString b64 = m_pendingCanvasInsert.b64;
        const double largoCm = m_pendingCanvasInsert.largoCm;
        const double anchoCm = m_pendingCanvasInsert.anchoCm;
        const bool hasLargo = m_pendingCanvasInsert.hasLargo;
        const bool hasAncho = m_pendingCanvasInsert.hasAncho;

        if (ok && m_topBoard) {
            const QString nom =
                m_nombrePorPiezaId.value(job.piezaId, QStringLiteral("#%1").arg(job.piezaId));
            m_topBoard->addPiezaImage(b64, largoCm, anchoCm, hasLargo, hasAncho, job.posCm, newUsadaId,
                                      job.piezaId, nom);
        }
        else if (!ok)
            QMessageBox::warning(this, tr("Timelines"),
                                 tr("No se pudo guardar la pieza en el tablero: %1").arg(err));
        if (ok && m_usada)
            m_usada->fetchPiezasNombresPorTimeline(m_timelineId);

        m_previewInFlight = false;
        m_activePreviewPiezaId = 0;
        pumpPlacementPreviewIfIdle();
    }

    void applyCanvasPlacementsFromDb(int timelineId, const QVector<UsadaCanvasPlacement>& rows)
    {
        if (timelineId != m_timelineId)
            return;
        for (const UsadaCanvasPlacement& r : rows) {
            if (r.piezaId > 0 && r.usadaId > 0)
                enqueuePlacementPreview(
                    {PlacementJobKind::CanvasFromDb, r.piezaId, QPointF(r.xCm, r.yCm), r.usadaId});
        }
    }

    void setTimeline(const Timeline& timeline)
    {
        m_timelineId = timeline.id;
        m_usedPiezaIds.clear();
        m_allPiezaIds.clear();
        m_allPiezaNombres.clear();
        m_allPiezaTooltips.clear();
        m_codigoByPiezaId.clear();
        m_nombrePorPiezaId.clear();
        m_usedListNombres.clear();
        m_usedListIds.clear();
        m_usedListTooltips.clear();
        m_tiposByPiezaId.clear();
        m_gotUsadas = false;
        m_gotTodasPiezas = false;
        if (m_pieceFilterSearch) {
            QSignalBlocker b(m_pieceFilterSearch);
            m_pieceFilterSearch->clear();
        }
        if (m_pieceFilterTipoCombo) {
            QSignalBlocker bt(m_pieceFilterTipoCombo);
            m_pieceFilterTipoCombo->setCurrentIndex(0);
        }

        m_previewJobs.clear();
        m_previewInFlight = false;
        m_activePreviewPiezaId = 0;
        m_pendingCanvasInsert.active = false;
        if (m_topBoard) {
            const double L = timeline.dimensionesLongitudCm > 0 ? timeline.dimensionesLongitudCm : kDefaultLongitudCm;
            const double W = timeline.dimensionesAnchuraCm > 0 ? timeline.dimensionesAnchuraCm : kDefaultAnchuraCm;
            m_topBoard->setBoardCm(L, W);
        }

        const QString esc = timeline.name.toHtmlEscaped();
        m_heading->setText(QStringLiteral("%1 <b>%2</b>")
                                 .arg(tr("Modificar timeline"), esc));
        if (m_usedPiecesList && m_usedPiecesEmpty) {
            m_usedPiecesList->clear();
            m_usedPiecesList->hide();
            m_usedPiecesEmpty->setText(tr("Cargando\u2026"));
            m_usedPiecesEmpty->show();
        }
        if (m_unusedPiecesList && m_unusedPiecesEmpty) {
            m_unusedPiecesList->clear();
            m_unusedPiecesList->hide();
            m_unusedPiecesEmpty->setText(tr("Cargando\u2026"));
            m_unusedPiecesEmpty->show();
        }

        if (timeline.id <= 0) {
            m_gotUsadas = true;
            m_gotTodasPiezas = true;
            rebuildUsedPiecesList();
            rebuildNoUsadasList();
            return;
        }
        if (m_usada) {
            m_usada->fetchPiezasNombresPorTimeline(timeline.id);
            m_usada->fetchCanvasPlacementsForTimeline(timeline.id);
        } else {
            m_gotUsadas = true;
            if (m_usedPiecesList && m_usedPiecesEmpty) {
                m_usedPiecesList->clear();
                m_usedPiecesList->hide();
                m_usedPiecesEmpty->setText(tr("No hay piezas usadas en este timeline."));
                m_usedPiecesEmpty->show();
            }
        }
        if (m_pieza)
            m_pieza->fetchTodosIdNombre(timeline.id);
        else
            m_gotTodasPiezas = true;
        rebuildNoUsadasList();
    }

    void applyPiezasUsadasList(int timelineId, const QStringList& nombres, const QVector<int>& piezaIds,
                               const QStringList& tooltips)
    {
        if (timelineId != m_timelineId || !m_usedPiecesList || !m_usedPiecesEmpty)
            return;
        m_usedPiezaIds.clear();
        for (int pid : piezaIds) {
            if (pid > 0)
                m_usedPiezaIds.insert(pid);
        }
        m_gotUsadas = true;

        m_usedListNombres = nombres;
        m_usedListIds.clear();
        m_usedListIds.reserve(nombres.size());
        for (int i = 0; i < nombres.size(); ++i)
            m_usedListIds.append(i < piezaIds.size() ? piezaIds.at(i) : 0);
        m_usedListTooltips = tooltips;
        while (m_usedListTooltips.size() < m_usedListNombres.size())
            m_usedListTooltips.append(QString());

        for (int i = 0; i < m_usedListNombres.size(); ++i) {
            const int pid = m_usedListIds.value(i, 0);
            if (pid > 0)
                m_nombrePorPiezaId.insert(pid, m_usedListNombres.at(i));
        }

        rebuildUsedPiecesList();
        rebuildNoUsadasList();
    }

    void applyTodosPiezasIdNombre(int timelineContext, const QVector<int>& ids, const QStringList& nombres,
                                  const QStringList& codigos, const QStringList& tooltips,
                                  const QVariantList& tipoIdsPorPieza)
    {
        if (timelineContext != m_timelineId || !m_unusedPiecesList || !m_unusedPiecesEmpty)
            return;
        m_allPiezaIds = ids;
        m_allPiezaNombres = nombres;
        m_allPiezaTooltips = tooltips;
        m_gotTodasPiezas = true;
        m_tiposByPiezaId.clear();
        m_codigoByPiezaId.clear();
        m_nombrePorPiezaId.clear();
        for (int i = 0; i < ids.size(); ++i) {
            const int pid = ids[i];
            if (pid > 0 && i < nombres.size())
                m_nombrePorPiezaId.insert(pid, nombres.at(i));
            if (pid > 0 && i < codigos.size() && !codigos.at(i).isEmpty())
                m_codigoByPiezaId.insert(pid, codigos.at(i));
            QVector<int> tids;
            if (i < tipoIdsPorPieza.size()) {
                const QVariant cell = tipoIdsPorPieza.at(i);
                const QVariantList row = cell.toList();
                tids.reserve(row.size());
                for (const QVariant& tv : row) {
                    int tid = 0;
                    if (tv.canConvert<int>())
                        tid = tv.toInt();
                    else if (tv.canConvert<double>())
                        tid = static_cast<int>(tv.toDouble());
                    else {
                        bool tok = false;
                        tid = tv.toInt(&tok);
                        if (!tok)
                            tid = static_cast<int>(tv.toDouble(&tok));
                    }
                    if (tid > 0)
                        tids.append(tid);
                }
            }
            m_tiposByPiezaId.insert(pid, tids);
        }
        rebuildNoUsadasList();
        rebuildUsedPiecesList();
    }

    void refillTipoFilterCombo(const QVector<Tipo>& items)
    {
        if (!m_pieceFilterTipoCombo)
            return;
        const QVariant prevData = m_pieceFilterTipoCombo->currentData();
        const qlonglong prevTipoId = prevData.toLongLong();
        QSignalBlocker b(m_pieceFilterTipoCombo);
        m_pieceFilterTipoCombo->clear();
        m_pieceFilterTipoCombo->addItem(tr("Tipo: Todos"), QVariant::fromValue<qlonglong>(0));
        for (const Tipo& t : items) {
            const QString label = t.nombre.isEmpty() ? QString::number(t.id) : t.nombre;
            m_pieceFilterTipoCombo->addItem(label, QVariant::fromValue<qlonglong>(t.id));
        }
        int idxRestored = -1;
        for (int i = 0; i < m_pieceFilterTipoCombo->count(); ++i) {
            if (m_pieceFilterTipoCombo->itemData(i).toLongLong() == prevTipoId) {
                idxRestored = i;
                break;
            }
        }
        m_pieceFilterTipoCombo->setCurrentIndex(idxRestored >= 0 ? idxRestored : 0);
        rebuildUsedPiecesList();
        rebuildNoUsadasList();
    }

    void onCanvasPieceSelected(int piezaId, int usadaRowId)
    {
        m_selectedCanvasPiezaId = piezaId;
        m_selectedCanvasUsadaId = usadaRowId;
        if (piezaId <= 0 || !m_pieza) {
            clearPiecePropertiesPanel();
            return;
        }
        if (m_propsPlaceholder) {
            m_propsPlaceholder->setText(tr("Cargando datos de la pieza\u2026"));
            m_propsPlaceholder->show();
        }
        if (m_propsImageLabel) {
            m_propsImageLabel->hide();
            m_propsImageLabel->clear();
        }
        if (m_propsForm) {
            while (m_propsForm->rowCount() > 0)
                m_propsForm->removeRow(0);
        }
        if (m_propsFormHost)
            m_propsFormHost->hide();
        if (m_propsRemoveFromCanvasBtn)
            m_propsRemoveFromCanvasBtn->setEnabled(false);
        m_pieza->fetchPiezaForEdit(piezaId);
    }

    void onPiezaDetailsForPropertiesPanel(const PiezaCardData& card)
    {
        if (m_selectedCanvasPiezaId <= 0)
            return;
        if (card.pieza.id <= 0 || card.pieza.id != m_selectedCanvasPiezaId) {
            if (m_propsPlaceholder)
                m_propsPlaceholder->setText(tr("No se pudieron cargar los datos de la pieza."));
            if (m_propsPlaceholder)
                m_propsPlaceholder->show();
            if (m_propsFormHost)
                m_propsFormHost->hide();
            if (m_propsImageLabel)
                m_propsImageLabel->hide();
            if (m_propsRemoveFromCanvasBtn)
                m_propsRemoveFromCanvasBtn->setEnabled(false);
            return;
        }
        applyPieceCardToPropertiesPanel(card);
    }

    void setOnBack(std::function<void()> fn) { m_onBack = std::move(fn); }
    void setOnExport(std::function<void()> fn) { m_onExport = std::move(fn); }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const int w = m_mainSplit ? m_mainSplit->width() : 0;
        const int h = m_leftSplit ? m_leftSplit->height() : 0;
        if (!m_initialSplitGeomDone && m_mainSplit && m_leftSplit && w > 160 && h > 80) {
            m_initialSplitGeomDone = true;
            const int side = qBound(180, w / 4, 420);
            m_mainSplit->setSizes({ qMax(160, w - side), side });
            const int half = h / 2;
            m_leftSplit->setSizes({ qMax(80, half), qMax(80, h - half) });
        }
        if (!m_sideVertSplitInitialDone && m_sideVertSplit) {
            const int sh = m_sideVertSplit->height();
            if (sh > 120) {
                m_sideVertSplitInitialDone = true;
                const int handle = m_sideVertSplit->handleWidth();
                const int avail = qMax(0, sh - handle);
                const int topH = (avail * 2) / 3;
                const int bottomH = avail - topH;
                m_sideVertSplit->setSizes({ qMax(1, topH), qMax(1, bottomH) });
            }
        }
    }

private:
    static int tipoFilterIdFromCombo(const QComboBox* combo)
    {
        if (!combo || combo->currentIndex() < 0)
            return 0;
        const QVariant d = combo->currentData();
        if (!d.isValid())
            return 0;
        bool ok = false;
        const int v = static_cast<int>(d.toLongLong(&ok));
        return ok && v > 0 ? v : 0;
    }

    bool pieceMatchesFilters(int piezaId, const QString& nombre) const
    {
        const QString q = m_pieceFilterSearch ? m_pieceFilterSearch->text().trimmed() : QString();
        if (!q.isEmpty()) {
            const bool nameHit = nombre.contains(q, Qt::CaseInsensitive);
            const QString codigo = m_codigoByPiezaId.value(piezaId);
            const bool codeHit = !codigo.isEmpty() && codigo.contains(q, Qt::CaseInsensitive);
            if (!nameHit && !codeHit)
                return false;
        }
        const int tipoF = tipoFilterIdFromCombo(m_pieceFilterTipoCombo);
        if (tipoF <= 0)
            return true;
        const QVector<int> tids = m_tiposByPiezaId.value(piezaId);
        return tids.contains(tipoF);
    }

    void rebuildUsedPiecesList()
    {
        if (!m_usedPiecesList || !m_usedPiecesEmpty)
            return;
        if (m_timelineId <= 0) {
            m_usedPiecesList->clear();
            m_usedPiecesList->hide();
            m_usedPiecesEmpty->setText(tr("No hay timeline activo."));
            m_usedPiecesEmpty->show();
            return;
        }
        if (!m_gotUsadas) {
            return;
        }
        m_usedPiecesList->clear();
        const int n = m_usedListNombres.size();
        for (int i = 0; i < n; ++i) {
            const int pid = m_usedListIds.value(i, 0);
            const QString& nom = m_usedListNombres.at(i);
            if (!pieceMatchesFilters(pid, nom))
                continue;
            auto* it = new QListWidgetItem(nom);
            if (pid > 0)
                it->setData(kPiezaListIdRole, pid);
            if (i < m_usedListTooltips.size() && !m_usedListTooltips.at(i).isEmpty())
                it->setToolTip(m_usedListTooltips.at(i));
            m_usedPiecesList->addItem(it);
        }
        if (n == 0) {
            m_usedPiecesEmpty->setText(tr("No hay piezas usadas en este timeline."));
            m_usedPiecesEmpty->show();
            m_usedPiecesList->hide();
        } else if (m_usedPiecesList->count() == 0) {
            m_usedPiecesEmpty->setText(tr("Ninguna pieza coincide con el filtro."));
            m_usedPiecesEmpty->show();
            m_usedPiecesList->hide();
        } else {
            m_usedPiecesEmpty->hide();
            m_usedPiecesList->show();
        }
    }

    void rebuildNoUsadasList()
    {
        if (!m_unusedPiecesList || !m_unusedPiecesEmpty)
            return;
        if (m_timelineId <= 0) {
            m_unusedPiecesList->clear();
            m_unusedPiecesList->hide();
            m_unusedPiecesEmpty->setText(tr("No hay timeline activo."));
            m_unusedPiecesEmpty->show();
            return;
        }
        if (!m_gotUsadas || !m_gotTodasPiezas) {
            m_unusedPiecesList->clear();
            m_unusedPiecesList->hide();
            m_unusedPiecesEmpty->setText(tr("Cargando\u2026"));
            m_unusedPiecesEmpty->show();
            return;
        }
        m_unusedPiecesList->clear();
        const int n = m_allPiezaIds.size();
        int totalUnused = 0;
        for (int i = 0; i < n; ++i) {
            if (m_usedPiezaIds.contains(m_allPiezaIds[i]))
                continue;
            ++totalUnused;
            if (!pieceMatchesFilters(m_allPiezaIds[i], m_allPiezaNombres.at(i)))
                continue;
            auto* it = new QListWidgetItem(m_allPiezaNombres.at(i));
            it->setData(kPiezaListIdRole, m_allPiezaIds[i]);
            if (i < m_allPiezaTooltips.size() && !m_allPiezaTooltips.at(i).isEmpty())
                it->setToolTip(m_allPiezaTooltips.at(i));
            m_unusedPiecesList->addItem(it);
        }
        if (totalUnused == 0) {
            m_unusedPiecesEmpty->setText(tr("Todas las piezas est\u00e1n usadas en este timeline."));
            m_unusedPiecesEmpty->show();
            m_unusedPiecesList->hide();
        } else if (m_unusedPiecesList->count() == 0) {
            m_unusedPiecesEmpty->setText(tr("Ninguna pieza coincide con el filtro."));
            m_unusedPiecesEmpty->show();
            m_unusedPiecesList->hide();
        } else {
            m_unusedPiecesEmpty->hide();
            m_unusedPiecesList->show();
        }
    }

    void clearPiecePropertiesPanel()
    {
        m_selectedCanvasPiezaId = 0;
        m_selectedCanvasUsadaId = 0;
        if (!m_propsPlaceholder || !m_propsForm || !m_propsFormHost || !m_propsImageLabel
            || !m_propsRemoveFromCanvasBtn)
            return;
        m_propsPlaceholder->setText(tr("Seleccione una pieza en el tablero superior."));
        m_propsPlaceholder->show();
        m_propsImageLabel->hide();
        m_propsImageLabel->clear();
        while (m_propsForm->rowCount() > 0)
            m_propsForm->removeRow(0);
        m_propsFormHost->hide();
        m_propsRemoveFromCanvasBtn->setEnabled(false);
    }

    void applyPieceCardToPropertiesPanel(const PiezaCardData& card)
    {
        if (!m_propsForm || !m_propsPlaceholder || !m_propsFormHost || !m_propsImageLabel
            || !m_propsRemoveFromCanvasBtn)
            return;
        while (m_propsForm->rowCount() > 0)
            m_propsForm->removeRow(0);

        const auto& p = card.pieza;
        QPixmap pm;
        if (!card.imagenesBase64.isEmpty()) {
            const QByteArray raw = QByteArray::fromBase64(card.imagenesBase64.first().toLatin1());
            pm.loadFromData(raw);
        }
        if (!pm.isNull()) {
            m_propsImageLabel->setPixmap(
                pm.scaled(220, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_propsImageLabel->show();
        } else {
            m_propsImageLabel->hide();
        }

        addPropsFormRow(m_propsForm, tr("C\u00f3digo"), p.codigo.isEmpty() ? QStringLiteral("\u2014") : p.codigo);
        addPropsFormRow(m_propsForm, tr("Nombre"), p.nombre.isEmpty() ? QStringLiteral("\u2014") : p.nombre);
        addPropsFormRow(m_propsForm, tr("Notas"),
                        p.notas.isEmpty() ? QStringLiteral("\u2014") : p.notas);

        QString dimStr;
        if (p.hasLargoCm || p.hasAnchoCm || p.hasAltoCm) {
            QStringList parts;
            if (p.hasLargoCm)
                parts.append(tr("L %1 cm").arg(QLocale().toString(p.largoCm, 'f', 1)));
            if (p.hasAnchoCm)
                parts.append(tr("An %1 cm").arg(QLocale().toString(p.anchoCm, 'f', 1)));
            if (p.hasAltoCm)
                parts.append(tr("Al %1 cm").arg(QLocale().toString(p.altoCm, 'f', 1)));
            dimStr = parts.join(QStringLiteral(" \u00B7 "));
        } else {
            dimStr = QStringLiteral("\u2014");
        }
        addPropsFormRow(m_propsForm, tr("Dimensiones (cm)"), dimStr);

        addPropsFormRow(m_propsForm, tr("Precio"), QLocale().toString(p.precio, 'f', 2));
        const QString fechaStr =
            p.fecha.isValid() ? QLocale().toString(p.fecha, QLocale::ShortFormat) : QStringLiteral("\u2014");
        addPropsFormRow(m_propsForm, tr("Fecha"), fechaStr);

        QStringList nombresTipo;
        nombresTipo.reserve(card.tipos.size());
        for (const QString& t : card.tipos) {
            const QString nt = t.trimmed();
            if (!nt.isEmpty())
                nombresTipo.append(nt);
        }
        const QString tiposStr =
            nombresTipo.isEmpty() ? QStringLiteral("\u2014") : nombresTipo.join(QStringLiteral(", "));
        addPropsFormRow(m_propsForm, tr("Tipos"), tiposStr);

        const QString propStr =
            card.propietarioNombre.trimmed().isEmpty() ? QStringLiteral("\u2014") : card.propietarioNombre;
        const QString ubiStr =
            card.ubicacionNombre.trimmed().isEmpty() ? QStringLiteral("\u2014") : card.ubicacionNombre;
        addPropsFormRow(m_propsForm, tr("Propietario"), propStr);
        addPropsFormRow(m_propsForm, tr("Ubicaci\u00f3n"), ubiStr);

        addPiezaParamsToPropsForm(m_propsForm, card, this);

        m_propsPlaceholder->hide();
        m_propsFormHost->show();
        m_propsRemoveFromCanvasBtn->setEnabled(m_selectedCanvasUsadaId > 0);
    }

    UsadaService* m_usada;
    PiezaService* m_pieza;
    int m_timelineId;
    QSet<int> m_usedPiezaIds;
    QVector<int> m_allPiezaIds;
    QStringList m_allPiezaNombres;
    QStringList m_allPiezaTooltips;
    QHash<int, QVector<int>> m_tiposByPiezaId;
    QHash<int, QString> m_codigoByPiezaId;
    QHash<int, QString> m_nombrePorPiezaId;
    QStringList m_usedListNombres;
    QVector<int> m_usedListIds;
    QStringList m_usedListTooltips;
    bool m_gotUsadas;
    bool m_gotTodasPiezas;
    QLabel* m_heading;
    QPushButton* m_backBtn;
    QPushButton* m_exportBtn;
    TimelineTopBoardView* m_topBoard;
    TimelineCanvasWidget* m_canvasBottom;
    QQueue<PlacementPreviewJob> m_previewJobs;
    bool m_previewInFlight = false;
    int m_activePreviewPiezaId = 0;
    int m_selectedCanvasPiezaId = 0;
    int m_selectedCanvasUsadaId = 0;
    QScrollArea* m_propsScroll = nullptr;
    QWidget* m_propsInner = nullptr;
    QLabel* m_propsPlaceholder = nullptr;
    QLabel* m_propsImageLabel = nullptr;
    QWidget* m_propsFormHost = nullptr;
    QFormLayout* m_propsForm = nullptr;
    QPushButton* m_propsRemoveFromCanvasBtn = nullptr;
    struct PendingCanvasInsert {
        bool active = false;
        PlacementPreviewJob job;
        QString b64;
        double largoCm = 0;
        double anchoCm = 0;
        bool hasLargo = false;
        bool hasAncho = false;
    } m_pendingCanvasInsert;
    QWidget* m_sidePanel;
    QSplitter* m_mainSplit;
    QSplitter* m_leftSplit;
    QSplitter* m_sideVertSplit;
    QListWidget* m_usedPiecesList;
    QLabel* m_usedPiecesEmpty;
    QListWidget* m_unusedPiecesList;
    QLabel* m_unusedPiecesEmpty;
    QLineEdit* m_pieceFilterSearch;
    QComboBox* m_pieceFilterTipoCombo;
    bool m_initialSplitGeomDone;
    bool m_sideVertSplitInitialDone;
    std::function<void()> m_onBack;
    std::function<void()> m_onExport;
};

} // namespace

TimelinesPage::TimelinesPage(SupabaseClient* supabase, HistorialService* historial,
                             QWidget* parent)
    : QWidget(parent)
    , m_supabase(supabase)
    , m_service(new TimelineService(supabase, historial, this))
    , m_usadaService(new UsadaService(supabase, this))
    , m_piezaService(new PiezaService(supabase, historial, this))
    , m_tipoService(new TipoService(supabase, historial, this))
    , m_stack(new QStackedWidget(this))
    , m_listPage(new QWidget(this))
    , m_editorPage(new TimelineEditorWidget(m_usadaService, m_piezaService, this))
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_stack);

    auto* layout = new QVBoxLayout(m_listPage);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(tr("Timelines"), m_listPage);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    layout->addWidget(title);

    auto* desc = new QLabel(
        tr("Crea o edita l\u00edneas de tiempo: nombre y dimensiones del tablero superior en cent\u00edmetros. "
           "En el editor, arrastra piezas no usadas al tablero."),
        m_listPage);
    desc->setStyleSheet(QStringLiteral("font-size: 13px; color: #7f8c8d;"));
    layout->addWidget(desc);
    layout->addSpacing(16);

    auto* btnLayout = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("A\u00f1adir"), m_listPage);
    m_editBtn = new QPushButton(tr("Editar"), m_listPage);
    m_deleteBtn = new QPushButton(tr("Eliminar"), m_listPage);
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

    m_table = new QTableWidget(m_listPage);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        { tr("Id"), tr("Nombre"), tr("Tablero (cm)"), tr("Acciones") });
    m_table->setColumnHidden(0, true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
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

    m_countLabel = new QLabel(m_listPage);
    m_countLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    layout->addWidget(m_countLabel, 0, Qt::AlignRight);

    m_stack->addWidget(m_listPage);
    m_stack->addWidget(m_editorPage);

    m_overlay = new LoadingOverlay(this);

    auto* editor = static_cast<TimelineEditorWidget*>(m_editorPage);
    editor->setOnBack([this]() { m_stack->setCurrentWidget(m_listPage); });
    editor->setOnExport([]() {});

    connect(m_usadaService, &UsadaService::piezasUsadasPorTimelineReady, this,
        [this](int timelineId, const QStringList& nombres, const QVector<int>& piezaIds,
               const QStringList& tooltips) {
            static_cast<TimelineEditorWidget*>(m_editorPage)->applyPiezasUsadasList(timelineId, nombres,
                                                                                     piezaIds, tooltips);
        },
        Qt::QueuedConnection);
    connect(m_usadaService, &UsadaService::errorOccurred, this, [this](const QString& msg) {
        if (m_stack->currentWidget() == m_editorPage)
            QMessageBox::critical(this, tr("Timelines"), msg);
    },
        Qt::QueuedConnection);
    connect(m_usadaService, &UsadaService::canvasPlacementsReady, this,
        [this](int timelineId, const QVariantList& rows) {
            QVector<UsadaCanvasPlacement> parsed;
            parsed.reserve(rows.size());
            for (const QVariant& qv : rows) {
                const QVariantMap m = qv.toMap();
                UsadaCanvasPlacement r;
                r.usadaId = m.value(QStringLiteral("usadaId")).toInt();
                r.piezaId = m.value(QStringLiteral("piezaId")).toInt();
                r.xCm = m.value(QStringLiteral("xCm")).toDouble();
                r.yCm = m.value(QStringLiteral("yCm")).toDouble();
                if (r.piezaId > 0 && r.usadaId > 0)
                    parsed.append(r);
            }
            static_cast<TimelineEditorWidget*>(m_editorPage)->applyCanvasPlacementsFromDb(timelineId, parsed);
        },
        Qt::QueuedConnection);
    connect(m_piezaService, &PiezaService::todosPiezasIdNombreReady, this,
        [this](int timelineContext, const QVector<int>& ids, const QStringList& nombres,
               const QStringList& codigos, const QStringList& tooltips,
               const QVariantList& tipoIdsPorPieza) {
            static_cast<TimelineEditorWidget*>(m_editorPage)
                ->applyTodosPiezasIdNombre(timelineContext, ids, nombres, codigos, tooltips,
                                          tipoIdsPorPieza);
        },
        Qt::QueuedConnection);
    connect(m_tipoService, &TipoService::dataReady, this, [this](const QVector<Tipo>& items) {
        static_cast<TimelineEditorWidget*>(m_editorPage)->refillTipoFilterCombo(items);
    });
    connect(m_tipoService, &TipoService::errorOccurred, this, [this](const QString& msg) {
        if (m_stack->currentWidget() == m_editorPage)
            QMessageBox::critical(this, tr("Timelines"), msg);
    });
    connect(m_piezaService, &PiezaService::errorOccurred, this, [this](const QString& msg) {
        if (m_stack->currentWidget() == m_editorPage)
            QMessageBox::critical(this, tr("Timelines"), msg);
    },
        Qt::QueuedConnection);
    connect(m_piezaService, &PiezaService::piezaPlacementPreviewReady, this,
        [this](int piezaId, const QString& b64, double largoCm, double anchoCm, bool hasL, bool hasA,
               bool tieneAcciones) {
            static_cast<TimelineEditorWidget*>(m_editorPage)
                ->onPiezaPlacementPreviewFromService(piezaId, b64, largoCm, anchoCm, hasL, hasA,
                                                     tieneAcciones);
        },
        Qt::QueuedConnection);
    connect(m_piezaService, &PiezaService::piezaForEditReady, this,
        [this](const PiezaCardData& card) {
            static_cast<TimelineEditorWidget*>(m_editorPage)->onPiezaDetailsForPropertiesPanel(card);
        },
        Qt::QueuedConnection);

    connect(addBtn, &QPushButton::clicked, this, &TimelinesPage::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &TimelinesPage::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TimelinesPage::onDelete);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col == 3) return;
        int id = m_table->item(row, 0) ? m_table->item(row, 0)->text().toInt() : 0;
        if (id <= 0) return;
        QString nm = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();
        Timeline t = m_timelineCache.value(id);
        if (t.id <= 0) {
            t.id = id;
            t.name = nm;
            t.dimensionesLongitudCm = kDefaultLongitudCm;
            t.dimensionesAnchuraCm = kDefaultAnchuraCm;
        }
        openTimelineEditor(t);
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        bool has = selectedId() > 0;
        m_editBtn->setEnabled(has);
        m_deleteBtn->setEnabled(has);
    });

    connect(m_service, &TimelineService::dataReady, this, [this](const QVector<Timeline>& list) {
        m_overlay->hideOverlay();
        m_timelineCache.clear();
        for (const Timeline& t : list)
            m_timelineCache.insert(t.id, t);
        m_table->setRowCount(list.size());
        for (int i = 0; i < list.size(); ++i) {
            m_table->setItem(i, 0, new QTableWidgetItem(QString::number(list[i].id)));
            m_table->setItem(i, 1, new QTableWidgetItem(list[i].name));
            const double cw = list[i].dimensionesLongitudCm > 0 ? list[i].dimensionesLongitudCm : kDefaultLongitudCm;
            const double ch = list[i].dimensionesAnchuraCm > 0 ? list[i].dimensionesAnchuraCm : kDefaultAnchuraCm;
            auto* dimItem = new QTableWidgetItem(
                tr("%1 x %2 cm")
                    .arg(QLocale().toString(cw, 'f', 1))
                    .arg(QLocale().toString(ch, 'f', 1)));
            dimItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            m_table->setItem(i, 2, dimItem);
            auto* modBtn = new QPushButton(tr("Modificar"), m_table);
            modBtn->setCursor(Qt::PointingHandCursor);
            modBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #4a6fa5; color: white; border: none; "
                "padding: 6px 14px; border-radius: 4px; font-size: 12px; }"
                "QPushButton:hover { background-color: #5d82b8; }"));
            const Timeline tcopy = list[i];
            connect(modBtn, &QPushButton::clicked, this, [this, tcopy]() {
                openTimelineEditor(tcopy);
            });
            m_table->setCellWidget(i, 3, modBtn);
        }
        m_table->resizeColumnToContents(2);
        m_table->resizeColumnToContents(3);
        const int accionesW = qMax(m_table->columnWidth(3) * 2, 160);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        m_table->setColumnWidth(3, accionesW);
        m_countLabel->setText(tr("Mostrando %1 elementos").arg(list.size()));
    });
    connect(m_service, &TimelineService::mutationDone, this, &TimelinesPage::refreshTable);
    connect(m_service, &TimelineService::errorOccurred, this, [this](const QString& msg) {
        m_overlay->hideOverlay();
        QMessageBox::critical(this, tr("Timelines"), msg);
    });

    m_tipoService->fetchAll();

    refreshTable();
}

void TimelinesPage::openTimelineEditor(const Timeline& timeline)
{
    auto* editor = static_cast<TimelineEditorWidget*>(m_editorPage);
    editor->setTimeline(timeline);
    m_stack->setCurrentWidget(m_editorPage);
}

void TimelinesPage::refreshTable()
{
    m_overlay->showOverlay();
    m_service->fetchAll();
}

void TimelinesPage::onAdd()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("A\u00f1adir timeline"));
    auto* form = new QFormLayout(&dlg);
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr("Nombre del timeline"));
    nameEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nameEdit);
    auto* longitudSpin = new QDoubleSpinBox(&dlg);
    longitudSpin->setRange(kBoardSpinMinCm, kBoardSpinMaxCm);
    longitudSpin->setDecimals(2);
    longitudSpin->setValue(kDefaultLongitudCm);
    longitudSpin->setSuffix(tr(" cm"));
    form->addRow(tr("Longitud del tablero (canvas superior):"), longitudSpin);
    auto* anchuraSpin = new QDoubleSpinBox(&dlg);
    anchuraSpin->setRange(kBoardSpinMinCm, kBoardSpinMaxCm);
    anchuraSpin->setDecimals(2);
    anchuraSpin->setValue(kDefaultAnchuraCm);
    anchuraSpin->setSuffix(tr(" cm"));
    form->addRow(tr("Anchura del tablero (canvas superior):"), anchuraSpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Timelines"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    m_overlay->showOverlay();
    m_service->create(name, longitudSpin->value(), anchuraSpin->value());
}

void TimelinesPage::onEdit()
{
    int id = selectedId();
    if (id <= 0) return;
    int row = m_table->currentRow();
    QString currentName = m_table->item(row, 1)->text();
    Timeline cached = m_timelineCache.value(id);
    if (cached.id <= 0) {
        cached.id = id;
        cached.name = currentName;
        cached.dimensionesLongitudCm = kDefaultLongitudCm;
        cached.dimensionesAnchuraCm = kDefaultAnchuraCm;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Editar timeline"));
    auto* form = new QFormLayout(&dlg);
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setText(cached.name);
    nameEdit->setMinimumWidth(320);
    form->addRow(tr("Nombre:"), nameEdit);
    auto* longitudSpin = new QDoubleSpinBox(&dlg);
    longitudSpin->setRange(kBoardSpinMinCm, kBoardSpinMaxCm);
    longitudSpin->setDecimals(2);
    longitudSpin->setValue(cached.dimensionesLongitudCm > 0 ? cached.dimensionesLongitudCm : kDefaultLongitudCm);
    longitudSpin->setSuffix(tr(" cm"));
    form->addRow(tr("Longitud del tablero (canvas superior):"), longitudSpin);
    auto* anchuraSpin = new QDoubleSpinBox(&dlg);
    anchuraSpin->setRange(kBoardSpinMinCm, kBoardSpinMaxCm);
    anchuraSpin->setDecimals(2);
    anchuraSpin->setValue(cached.dimensionesAnchuraCm > 0 ? cached.dimensionesAnchuraCm : kDefaultAnchuraCm);
    anchuraSpin->setSuffix(tr(" cm"));
    form->addRow(tr("Anchura del tablero (canvas superior):"), anchuraSpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Timelines"), tr("El nombre no puede estar vac\u00edo."));
        return;
    }
    m_overlay->showOverlay();
    m_service->update(id, name, longitudSpin->value(), anchuraSpin->value());
}

void TimelinesPage::onDelete()
{
    int id = selectedId();
    if (id <= 0) return;
    QString name = m_table->item(m_table->currentRow(), 1)->text();
    auto ret = QMessageBox::question(this, tr("Eliminar timeline"),
        tr("\u00bfEliminar \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_overlay->showOverlay();
    m_service->remove(id);
}

int TimelinesPage::selectedId() const
{
    int row = m_table->currentRow();
    if (row < 0) return 0;
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->text().toInt() : 0;
}
