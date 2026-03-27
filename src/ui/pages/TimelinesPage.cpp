#include "TimelinesPage.h"
#include <QVBoxLayout>
#include <QLabel>

TimelinesPage::TimelinesPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Timelines"), this);
    title->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: bold; color: #2c3e50;"));
    title->setAlignment(Qt::AlignCenter);

    auto* desc = new QLabel(tr("Configura las l\u00edneas de tiempo"), this);
    desc->setStyleSheet(QStringLiteral("font-size: 14px; color: #7f8c8d;"));
    desc->setAlignment(Qt::AlignCenter);

    auto* hint = new QLabel(tr("Pr\u00f3ximamente"), this);
    hint->setStyleSheet(QStringLiteral("font-size: 12px; color: #bdc3c7; font-style: italic;"));
    hint->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(desc);
    layout->addSpacing(8);
    layout->addWidget(hint);
}
