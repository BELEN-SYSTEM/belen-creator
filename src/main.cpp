#include <QApplication>
#include <QFont>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("BetlemSystem"));
    app.setApplicationName(QStringLiteral("BelenCreator"));
    app.setApplicationDisplayName(QStringLiteral("Belen Creator"));

    QFont font(QStringLiteral("Segoe UI"), 10);
    app.setFont(font);

    MainWindow window;
    window.resize(1000, 600);
    window.showMaximized();

    return app.exec();
}
