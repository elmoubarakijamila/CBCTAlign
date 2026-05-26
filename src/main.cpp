/**
 * @file main.cpp
 * @brief Point d'entrée CBCTAlign
 */
#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QMessageBox>
#include <iostream>

#include "gui/MainWindow.h"
#include "utils/Logger.h"

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    app.setApplicationName("CBCTAlign");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CBCTAlign Research");
    app.setStyle(QStyleFactory::create("Fusion"));

    std::cout << "\n"
        "  CBCTAlign v1.0 — Alignement Spatio-Temporel CBCT\n"
        "  Guide par Points Cephalometriques\n\n";

    CBCTAlign::Logger::instance().init("cbctalign.log");
    CBCTAlign::Logger::instance().info("CBCTAlign démarré v1.0");

    try {
        CBCTAlign::MainWindow w;
        w.show();
        return app.exec();
    } catch (const std::exception& e) {
        CBCTAlign::Logger::instance().error(QString("Fatal: %1").arg(e.what()));
        QMessageBox::critical(nullptr, "Erreur", e.what());
        return 1;
    }
}
