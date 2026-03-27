#include "DispositivosPage.h"
#include <QVBoxLayout>
#include <QLabel>

DispositivosPage::DispositivosPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Dispositivos"), this);
    title->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: bold; color: #2c3e50;"));
    title->setAlignment(Qt::AlignCenter);

    auto* desc = new QLabel(tr("Administra los dispositivos del sistema"), this);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #7f8c8d;"));
    desc->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(desc);
}
