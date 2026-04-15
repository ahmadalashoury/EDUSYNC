#include <QApplication>
#include <QStyleFactory>
#include <QSettings>
#include "MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    app.setOrganizationName("EduSync");
    app.setApplicationName("EduSync");
    app.setApplicationVersion("2.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    // Default theme preference
    {
        QSettings s;
        if (!s.contains("theme")) s.setValue("theme", "dark");
    }

    MainWindow window;
    window.show();

    return app.exec();
}
