#pragma once

#include <QWidget>

class QTimer;

class LoadingOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void showOverlay();
    void hideOverlay();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QTimer* m_timer;
    int m_angle = 0;
};
