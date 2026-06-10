#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FreeSpaceFinder");
    app.setOrganizationName("FreeSpaceFinder");
    app.setApplicationVersion("1.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();
    return app.exec();
}
