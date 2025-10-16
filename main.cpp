#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QThread>
#include <QFuture>
#include <QSettings> 
// #include <QtConcurrent>
// #include <QOpenGLWidget>
// #include <QOpenGLFunctions>
// #include <QOpenGLShaderProgram>
// #include <QOpenGLBuffer>
// #include <QOpenGLVertexArrayObject>
// #include <QOpenGLTexture>
// #include <QOpenGLFramebufferObject>
// #include <QOpenGLPaintDevice>
// #include <QOpenGLContext>
// #include <QOpenGLShader>
// #include <QOpenGLVersionProfile>
// #include <QOpenGLDebugLogger>
// #include <QOpenGLTimerQuery>
// #include <QOpenGLTimeMonitor>
 #include "UltraMainWindow.h"

int main(int argc, char** argv) {
QApplication app(argc, argv);
    // Identify app/org so QSettings goes to a stable plist
    QCoreApplication::setOrganizationName("EduSync");
    QCoreApplication::setApplicationName("EduSync");
    // First-run default: Dark theme
    {
        QSettings s;
        if (!s.contains("theme")) s.setValue("theme", "dark");
    }    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Set application properties for 30x better version
    app.setApplicationName("EduSync Pro - 30x Better");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("EduSync Pro");
    app.setOrganizationDomain("edusync.pro");
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set ultra-modern theme with glassmorphism
    
    // Create stunning dark palette with glassmorphism effects
    QPalette ultraDarkPalette;
    ultraDarkPalette.setColor(QPalette::Window, QColor(15, 15, 15));
    ultraDarkPalette.setColor(QPalette::WindowText, Qt::white);
    ultraDarkPalette.setColor(QPalette::Base, QColor(10, 10, 10));
    ultraDarkPalette.setColor(QPalette::AlternateBase, QColor(20, 20, 20));
    ultraDarkPalette.setColor(QPalette::ToolTipBase, QColor(30, 30, 30));
    ultraDarkPalette.setColor(QPalette::ToolTipText, Qt::white);
    ultraDarkPalette.setColor(QPalette::Text, Qt::white);
    ultraDarkPalette.setColor(QPalette::Button, QColor(25, 25, 25));
    ultraDarkPalette.setColor(QPalette::ButtonText, Qt::white);
    ultraDarkPalette.setColor(QPalette::BrightText, QColor(255, 215, 0));
    ultraDarkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    ultraDarkPalette.setColor(QPalette::Highlight, QColor(138, 43, 226));
    ultraDarkPalette.setColor(QPalette::HighlightedText, Qt::white);
    
    
   
    

    UltraMainWindow window;       // variable name is 'window'
    window.show();

    auto* backgroundTimer = new QTimer(&window);
    QObject::connect(backgroundTimer, &QTimer::timeout, [&window]() {
        // window.updateAdvancedFeatures();
    });
    backgroundTimer->start(1000);

    
    return app.exec();
}
