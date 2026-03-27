#include "LoadingOverlay.h"
#include <QPainter>
#include <QTimer>
#include <QEvent>
#include <QPen>
#include <cmath>

LoadingOverlay::LoadingOverlay(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setVisible(false);

    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 30) % 360;
        update();
    });

    if (parent)
        parent->installEventFilter(this);
}

void LoadingOverlay::showOverlay()
{
    if (parentWidget())
        resize(parentWidget()->size());
    raise();
    setVisible(true);
    m_angle = 0;
    m_timer->start(50);
}

void LoadingOverlay::hideOverlay()
{
    setVisible(false);
    m_timer->stop();
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        resize(parentWidget()->size());
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), QColor(255, 255, 255, 180));

    const int arcSize = 40;
    QRect arcRect(width() / 2 - arcSize / 2, height() / 2 - arcSize / 2, arcSize, arcSize);

    QPen pen(QColor(0x40, 0x9e, 0xff), 4);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawArc(arcRect, m_angle * 16, 270 * 16);
}
