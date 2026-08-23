#include "Application.h"
#include <FluentQt/FluentQt.h>
#include "core/settings/SettingsRegistry.h"

namespace app {
    Application::Application(int &argc, char **argv) {
        fluent::prepareHighDpiApplication();
        m_qapp = std::make_unique<QApplication>(argc, argv);

        initializeResources();
        initializeSettings();
        initializeDatabase();
    }

    Application::~Application() = default;

    void Application::initializeResources() {
        fluent::initializeResources();
        if (m_qapp) {
            m_qapp->setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
        }
    }

    void Application::initializeSettings() {
        core::settings::SettingsRegistry::instance().loadAll();
    }

    void Application::initializeDatabase() {
        m_context.dbManager().initialize();
        if (auto *repo = dynamic_cast<data::repository::SqliteConversationRepository *>(m_context.conversationRepository())) {
            repo->initializeDatabase();
        }
    }

    int Application::run() {
        m_mainWindow = std::make_unique<ui::screen::main::MainWindow>(
            m_context.chatUseCases()
        );
        m_mainWindow->show();
        return m_qapp->exec();
    }
} // namespace app
