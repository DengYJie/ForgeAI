#include <QtWidgets/QApplication>
#include <FluentQt/FluentQt.h>
#include "core/settings/SettingsRegistry.h"
#include "app/ApplicationContext.h"
#include "ui/screen/main/MainWindow.h"

int main(int argc, char *argv[]) {
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    app.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    core::settings::SettingsRegistry::instance().loadAll();

    // 应用程序组合根 (Composition Root)
    app::ApplicationContext context;

    ui::screen::main::MainWindow mainWindow(
        context.chatService(),
        context.conversationService()
    );
    mainWindow.show();
    return app.exec();
}
