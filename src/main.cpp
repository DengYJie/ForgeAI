#include <QtWidgets/QApplication>
#include <FluentQt/FluentQt.h>
#include "core/settings/SettingsRegistry.h"
#include "ui/screen/main/MainWindow.h"

int main(int argc, char *argv[]) {
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    app.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    core::settings::SettingsRegistry::instance().loadAll();

    ui::screen::main::MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
