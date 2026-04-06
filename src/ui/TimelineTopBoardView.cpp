#include "TimelineTopBoardView.h"
#include <QBrush>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QWheelEvent>

namespace {

constexpr char kMimePiezaId[] = "application/x-belencreator-pieza-id";

/** Pieza en el tablero: borde negro, selección roja, arrastrable dentro del rectángulo de escena. */
class BoardPiecePixmapItem : public QGraphicsPixmapItem
{
public:
    BoardPiecePixmapItem(TimelineTopBoardView* board, int usadaRowId, int piezaId, const QPixmap& pm)
        : QGraphicsPixmapItem(pm)
        , m_board(board)
        , m_usadaId(usadaRowId)
        , m_piezaId(piezaId)
    {
        setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
    }

    void setPieceSelected(bool sel)
    {
        if (m_selected == sel)
            return;
        m_selected = sel;
        update();
        if (sel && m_board && m_piezaId > 0)
            m_board->reportCanvasPieceSelected(m_piezaId, m_usadaId);
    }

    bool pieceSelected() const { return m_selected; }
    int usadaRowId() const { return m_usadaId; }

    int type() const override { return UserType + 1; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
        QGraphicsPixmapItem::paint(painter, option, widget);
        QPen pen(m_selected ? QColor(220, 40, 40) : Qt::black);
        pen.setCosmetic(true);
        pen.setWidthF(m_selected ? 3.0 : 2.0);
        painter->save();
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect());
        painter->restore();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && scene()) {
            m_dragPressCenterCm = mapToScene(boundingRect().center());
            const QList<QGraphicsItem*> items = scene()->items();
            for (QGraphicsItem* it : items) {
                if (it == this)
                    continue;
                if (auto* p = dynamic_cast<BoardPiecePixmapItem*>(it))
                    p->setPieceSelected(false);
            }
            setPieceSelected(true);
        }
        QGraphicsPixmapItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        QGraphicsPixmapItem::mouseReleaseEvent(event);
        if (event->button() != Qt::LeftButton || !m_board || m_usadaId <= 0)
            return;
        const QPointF endCm = mapToScene(boundingRect().center());
        if (QPointF(endCm - m_dragPressCenterCm).manhattanLength() > 0.05)
            m_board->reportCanvasPieceMoveFinished(m_usadaId, endCm);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
    {
        // Evitar el comportamiento por defecto (equivale a otro mousePress).
        event->accept();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionChange && scene() && m_board) {
            const QRectF bounds = m_board->boardContentRectCm();
            const QPointF newPos = value.toPointF();
            QRectF r = sceneBoundingRect();
            r.translate(newPos - pos());
            QPointF adj = newPos;
            if (r.left() < bounds.left())
                adj.rx() += bounds.left() - r.left();
            if (r.right() > bounds.right())
                adj.rx() += bounds.right() - r.right();
            if (r.top() < bounds.top())
                adj.ry() += bounds.top() - r.top();
            if (r.bottom() > bounds.bottom())
                adj.ry() += bounds.bottom() - r.bottom();
            return QVariant(adj);
        }
        return QGraphicsPixmapItem::itemChange(change, value);
    }

private:
    bool m_selected = false;
    TimelineTopBoardView* m_board = nullptr;
    int m_usadaId = 0;
    int m_piezaId = 0;
    QPointF m_dragPressCenterCm;
};

static void deselectAllBoardPieces(QGraphicsScene* scene, QGraphicsItem* boardFill, QGraphicsItem* boardFrame,
                                   TimelineTopBoardView* board)
{
    if (!scene)
        return;
    for (QGraphicsItem* it : scene->items()) {
        if (it == boardFill || it == boardFrame)
            continue;
        if (auto* p = dynamic_cast<BoardPiecePixmapItem*>(it))
            p->setPieceSelected(false);
    }
    if (board)
        board->reportCanvasPieceSelected(0, 0);
}

} // namespace

TimelineTopBoardView::TimelineTopBoardView(QWidget* parent)
    : QGraphicsView(parent)
{
    setObjectName(QStringLiteral("timelineTopBoard"));
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setAcceptDrops(true);
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "QGraphicsView#timelineTopBoard { background-color: #f0f2f5; border: 1px solid #cfd8dc; "
        "border-radius: 2px; }"));
    setMinimumSize(160, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setToolTip(tr("Rueda del rat\u00f3n: zoom. Bot\u00f3n central arrastrando: desplazar. "
                  "Arrastra piezas no usadas desde la lista."));
}

void TimelineTopBoardView::setBoardCm(double longitudCm, double anchuraCm)
{
    m_longitudCm = qBound(1.0, longitudCm, 50000.0);
    m_anchuraCm = qBound(1.0, anchuraCm, 50000.0);
    clearPiezas();
    // Margen alrededor del tablero para que el borde no quede pegado al viewport.
    const double minSide = qMin(m_anchuraCm, m_longitudCm);
    m_sceneMarginCm = qBound(2.0, minSide * 0.03, 30.0);
    // Ancho en pantalla (X) = anchura en BD; alto (Y) = longitud en BD.
    m_scene->setSceneRect(-m_sceneMarginCm, -m_sceneMarginCm,
                          m_anchuraCm + 2.0 * m_sceneMarginCm, m_longitudCm + 2.0 * m_sceneMarginCm);

    if (m_boardFill) {
        m_scene->removeItem(m_boardFill);
        delete m_boardFill;
        m_boardFill = nullptr;
    }
    if (m_boardFrame) {
        m_scene->removeItem(m_boardFrame);
        delete m_boardFrame;
        m_boardFrame = nullptr;
    }
    m_boardFill = m_scene->addRect(QRectF(0, 0, m_anchuraCm, m_longitudCm), QPen(Qt::NoPen),
                                     QBrush(QColor(248, 249, 251)));
    m_boardFrame = m_scene->addRect(QRectF(0, 0, m_anchuraCm, m_longitudCm),
                                      QPen(QColor(100, 120, 140), 0.08), QBrush(Qt::NoBrush));
    m_boardFill->setZValue(-1000);
    m_boardFrame->setZValue(-999);
    m_didInitialFit = false;
    fitBoardInView();
}

QRectF TimelineTopBoardView::boardContentRectCm() const
{
    return QRectF(0, 0, m_anchuraCm, m_longitudCm);
}

void TimelineTopBoardView::clearPiezas()
{
    if (!m_scene)
        return;
    reportCanvasPieceSelected(0, 0);
    QList<QGraphicsItem*> toRemove;
    for (QGraphicsItem* it : m_scene->items()) {
        if (it != m_boardFill && it != m_boardFrame)
            toRemove.append(it);
    }
    for (QGraphicsItem* it : toRemove) {
        m_scene->removeItem(it);
        delete it;
    }
}

void TimelineTopBoardView::removeCanvasPieceByUsadaId(int usadaId)
{
    if (!m_scene || usadaId <= 0)
        return;
    QList<QGraphicsItem*> toRemove;
    for (QGraphicsItem* it : m_scene->items()) {
        if (it == m_boardFill || it == m_boardFrame)
            continue;
        if (auto* p = dynamic_cast<BoardPiecePixmapItem*>(it)) {
            if (p->usadaRowId() == usadaId)
                toRemove.append(it);
        }
    }
    for (QGraphicsItem* it : toRemove) {
        m_scene->removeItem(it);
        delete it;
    }
    reportCanvasPieceSelected(0, 0);
}

void TimelineTopBoardView::reportCanvasPieceMoveFinished(int usadaId, QPointF centroCm)
{
    if (usadaId > 0 && m_canvasPieceMovedHandler)
        m_canvasPieceMovedHandler(usadaId, centroCm);
}

void TimelineTopBoardView::reportCanvasPieceSelected(int piezaId, int usadaRowId)
{
    if (m_canvasPieceSelectionHandler)
        m_canvasPieceSelectionHandler(piezaId, usadaRowId);
}

void TimelineTopBoardView::addPiezaImage(const QString& imagenBase64, double largoCm, double anchoCm,
                                         bool hasLargo, bool hasAncho, QPointF centroCm, int usadaRowId,
                                         int piezaId, const QString& piezaNombre)
{
    QPixmap pm;
    if (!imagenBase64.isEmpty()) {
        const QByteArray raw = QByteArray::fromBase64(imagenBase64.toLatin1());
        pm.loadFromData(raw);
    }
    if (pm.isNull()) {
        pm = QPixmap(96, 96);
        pm.fill(QColor(220, 224, 230));
    }
    const double pw = qMax(1.0, double(pm.width()));
    const double ph = qMax(1.0, double(pm.height()));
    double sw = 10.0;
    double sh = 10.0;
    // Eje X del tablero = ancho de la pieza; eje Y = largo (coherente con vista en planta).
    if (hasLargo && hasAncho) {
        sw = qMax(0.1, anchoCm);
        sh = qMax(0.1, largoCm);
    } else if (hasLargo) {
        sh = qMax(0.1, largoCm);
        sw = sh * pw / ph;
    } else if (hasAncho) {
        sw = qMax(0.1, anchoCm);
        sh = sw * ph / pw;
    } else {
        sh = sw * ph / pw;
    }

    auto* pixItem = new BoardPiecePixmapItem(this, usadaRowId, piezaId, pm);
    if (!piezaNombre.isEmpty())
        pixItem->setToolTip(piezaNombre);
    pixItem->setTransformationMode(Qt::SmoothTransformation);
    QTransform tr;
    tr.scale(sw / pw, sh / ph);
    pixItem->setTransform(tr);
    pixItem->setPos(centroCm.x() - sw / 2.0, centroCm.y() - sh / 2.0);
    pixItem->setZValue(0);
    m_scene->addItem(pixItem);
}

void TimelineTopBoardView::fitBoardInView()
{
    if (!m_scene || m_longitudCm <= 0 || m_anchuraCm <= 0)
        return;
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    m_didInitialFit = true;
}

void TimelineTopBoardView::wheelEvent(QWheelEvent* event)
{
    if (!m_scene)
        return;
    const double factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    scale(factor, factor);
    event->accept();
}

bool TimelineTopBoardView::mimeHasPiezaId(const QMimeData* mime)
{
    return mime && mime->hasFormat(QString::fromLatin1(kMimePiezaId));
}

int TimelineTopBoardView::piezaIdFromMime(const QMimeData* mime)
{
    if (!mime)
        return 0;
    const QByteArray raw = mime->data(QString::fromLatin1(kMimePiezaId));
    bool ok = false;
    const int id = raw.toInt(&ok);
    return ok && id > 0 ? id : 0;
}

void TimelineTopBoardView::dragEnterEvent(QDragEnterEvent* event)
{
    if (mimeHasPiezaId(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void TimelineTopBoardView::dragMoveEvent(QDragMoveEvent* event)
{
    if (mimeHasPiezaId(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void TimelineTopBoardView::dropEvent(QDropEvent* event)
{
    const int pid = piezaIdFromMime(event->mimeData());
    if (pid <= 0 || !m_dropHandler) {
        QGraphicsView::dropEvent(event);
        return;
    }
    const QPointF scenePos = mapToScene(event->position().toPoint());
    QRectF board(0, 0, m_anchuraCm, m_longitudCm);
    QPointF p = scenePos;
    if (!board.contains(p))
        p = QPointF(qBound(0.0, p.x(), m_anchuraCm), qBound(0.0, p.y(), m_longitudCm));
    m_dropHandler(pid, p);
    event->acceptProposedAction();
}

void TimelineTopBoardView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_didInitialFit && m_scene && width() > 50 && height() > 50 && m_longitudCm > 0 && m_anchuraCm > 0)
        fitBoardInView();
}

void TimelineTopBoardView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panAnchor = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && m_scene) {
        QGraphicsItem* top = itemAt(event->pos());
        if (!top || top == m_boardFill || top == m_boardFrame)
            deselectAllBoardPieces(m_scene, m_boardFill, m_boardFrame, this);
    }
    QGraphicsView::mousePressEvent(event);
}

void TimelineTopBoardView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint d = event->pos() - m_panAnchor;
        if (horizontalScrollBar())
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        if (verticalScrollBar())
            verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        m_panAnchor = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void TimelineTopBoardView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
