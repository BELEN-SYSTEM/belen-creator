#pragma once

#include <QGraphicsView>
#include <QGraphicsRectItem>

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    CanvasView(QWidget* parent = nullptr);

private:
    QGraphicsScene* scene;
};