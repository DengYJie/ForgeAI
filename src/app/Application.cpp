#include "Application.h"
#include <FluentQt/FluentQt.h>
#include "core/logging/LoggingService.h"
#include "core/settings/SettingsRegistry.h"
#include "data/repository/SqliteConversationRepository.h"
#include "data/repository/SqliteModelRepository.h"
#include "data/repository/SqliteProjectRepository.h"
#include "core/model/ModelRegistry.h"

namespace app {
    Application::Application(int &argc, char **argv) {
        fluent::prepareHighDpiApplication();
        m_qapp = std::make_unique<QApplication>(argc, argv);
        m_context = std::make_unique<ApplicationContext>();

        initializeResources();
        initializeSettings();
        initializeDatabase();
    }

    Application::~Application() {
        m_mainWindow.reset();
        m_context.reset();
        core::logging::LoggingService::instance().shutdown();
        m_qapp.reset();
    }

    void Application::initializeResources() {
        fluent::initializeResources();
        if (m_qapp) {
            m_qapp->setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
        }
    }

    void Application::initializeSettings() {
        if (m_context && m_context->settingsRegistry()) {
            m_context->settingsRegistry()->loadAll();
        }
    }

    void Application::initializeDatabase() {
        m_context->dbManager().initialize();
        if (auto *repo = dynamic_cast<data::repository::SqliteConversationRepository *>(m_context->conversationRepository())) {
            repo->initializeDatabase();
        }
        // Project repository is composed for WorkPage's project/session tree.
        // It shares the same initialized connection as conversations.
        if (auto *projectRepo = dynamic_cast<data::repository::SqliteProjectRepository *>(m_context->projectRepository())) {
            projectRepo->initializeDatabase();
        }
        if (auto *modelRepo = dynamic_cast<data::repository::SqliteModelRepository *>(m_context->modelRepository())) {
            modelRepo->initializeDatabase(QStringLiteral(":/config/api.json"), QStringLiteral(":/config/models.json"));
        }
        if (auto *modelReg = m_context->modelRegistry()) {
            modelReg->initialize(QStringLiteral(":/config/api.json"), QStringLiteral(":/config/models.json"));
        }
    }

    int Application::run() {
        m_mainWindow = std::make_unique<ui::screen::main::MainWindow>(
            m_context->mainViewModel(),
            m_context->chatViewModel(),
            m_context->workViewModel(),
            m_context->knowledgeViewModel(),
            m_context->settingsUiRegistry()
        );
        m_mainWindow->show();
        return m_qapp->exec();
    }
} // namespace app
