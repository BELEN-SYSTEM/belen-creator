#pragma once

#include <QGraphicsView>
#include <QMimeData>
#include <QPoint>
#include <functional>

class QGraphicsScene;
class QGraphicsRectItem;
class QMouseEvent;

/**
 * Tablero en cm: eje X de escena = anchura del timeline; eje Y = longitud (vista en planta).
 * Zoom con rueda; botón central para desplazar. Acepta soltar piezas (MIME).
 */
class TimelineTopBoardView : public QGraphicsView
{
public:
    using DropHandler = std::function<void(int piezaId, QPointF posicionCm)>;
    using CanvasPieceMovedHandler = std::function<void(int usadaId, QPointF centroCm)>;
    /** `piezaId` y fila `usada` en BD; (0,0) si no hay pieza seleccionada en el tablero. */
    using CanvasPieceSelectionHandler = std::function<void(int piezaId, int usadaRowId)>;

    explicit TimelineTopBoardView(QWidget* parent = nullptr);

    void setBoardCm(double longitudCm, double anchuraCm);
    void clearPiezas();
    /** Quita del canvas la pieza ligada a esa fila `usada` (colocación en tablero). */
    void removeCanvasPieceByUsadaId(int usadaId);
    void addPiezaImage(const QString& imagenBase64, double largoCm, double anchoCm, bool hasLargo,
                       bool hasAncho, QPointF centroCm, int usadaRowId = 0, int piezaId = 0,
                       const QString& piezaNombre = QString());
    void setDropHandler(DropHandler fn) { m_dropHandler = std::move(fn); }
    void setCanvasPieceMovedHandler(CanvasPieceMovedHandler fn)
    {
        m_canvasPieceMovedHandler = std::move(fn);
    }
    void setCanvasPieceSelectionHandler(CanvasPieceSelectionHandler fn)
    {
        m_canvasPieceSelectionHandler = std::move(fn);
    }

    /** Notifica fin de arrastre de una pieza con fila `usada` (centro en cm, escena). */
    void reportCanvasPieceMoveFinished(int usadaId, QPointF centroCm);
    void reportCanvasPieceSelected(int piezaId, int usadaRowId);

    /** Área útil del tablero en cm (sin margen visual). */
    QRectF boardContentRectCm() const;

    void fitBoardInView();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static bool mimeHasPiezaId(const QMimeData* mime);
    static int piezaIdFromMime(const QMimeData* mime);

    QGraphicsScene* m_scene = nullptr;
    QGraphicsRectItem* m_boardFill = nullptr;
    QGraphicsRectItem* m_boardFrame = nullptr;
    double m_longitudCm = 300;
    double m_anchuraCm = 200;
    /** Margen de escena alrededor del tablero (cm) para ver el marco al hacer zoom/fit. */
    double m_sceneMarginCm = 3.0;
    bool m_didInitialFit = false;
    DropHandler m_dropHandler;
    CanvasPieceMovedHandler m_canvasPieceMovedHandler;
    CanvasPieceSelectionHandler m_canvasPieceSelectionHandler;
    bool m_panning = false;
    QPoint m_panAnchor;
};
