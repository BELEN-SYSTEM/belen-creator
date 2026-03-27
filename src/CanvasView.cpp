#include "CanvasView.h"
#include <QGraphicsScene>
#include <QBrush>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    setScene(scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);

    // Crear un nodo de ejemplo
    QGraphicsRectItem* node = scene->addRect(0, 0, 150, 80);
    node->setBrush(QBrush(Qt::blue));
    node->setFlag(QGraphicsItem::ItemIsMovable);
    node->setFlag(QGraphicsItem::ItemIsSelectable);

    scene->setSceneRect(-500, -500, 1000, 1000);
}