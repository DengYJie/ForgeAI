#include "McpManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::mcp {

    McpManager::McpManager(QObject* parent)
        : QObject(parent), m_toolProvider(std::make_shared<McpToolProvider>()) {
    }

    McpManager::~McpManager() {
        stopAll();
    }

    QList<McpServerConfig> McpManager::parseConfigFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        const QString content = QString::fromUtf8(file.readAll());
        file.close();
        return parseConfigContent(content);
    }

    QList<McpServerConfig> McpManager::parseConfigContent(const QString& jsonContent) {
        QList<McpServerConfig> configs;
        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            return configs;
        }

        const auto rootObj = doc.object();
        const auto serversObj = rootObj.value(QStringLiteral("mcpServers")).toObject();

        for (auto it = serversObj.begin(); it != serversObj.end(); ++it) {
            const QString name = it.key();
            const auto serverObj = it.value().toObject();

            McpServerConfig cfg;
            cfg.name = name;
            cfg.command = serverObj.value(QStringLiteral("command")).toString();
            
            const auto argsArr = serverObj.value(QStringLiteral("args")).toArray();
            for (const auto& a : argsArr) {
                cfg.args.append(a.toString());
            }

            const auto envObj = serverObj.value(QStringLiteral("env")).toObject();
            for (auto envIt = envObj.begin(); envIt != envObj.end(); ++envIt) {
                cfg.env.insert(envIt.key(), envIt.value().toString());
            }

            cfg.cwd = serverObj.value(QStringLiteral("cwd")).toString();
            cfg.disabled = serverObj.value(QStringLiteral("disabled")).toBool(false);
            cfg.autoApprove = serverObj.value(QStringLiteral("autoApprove")).toBool(false);

            if (!cfg.command.isEmpty()) {
                configs.append(cfg);
            }
        }

        return configs;
    }

    void McpManager::registerServer(const McpServerConfig& config) {
        if (config.name.isEmpty()) return;

        if (m_sessions.contains(config.name)) {
            auto existing = m_sessions.value(config.name);
            if (existing && existing->config() == config &&
                (existing->state() == McpSessionState::Connected || existing->state() == McpSessionState::Connecting)) {
                return;
            }
            stopServer(config.name);
            m_toolProvider->removeSession(existing.get());
            m_sessions.remove(config.name);
        }

        auto session = std::make_shared<McpSession>(config, this);
        connect(session.get(), &McpSession::errorOccurred, this, [this, name = config.name](const QString& err) {
            emit serverError(name, err);
        });

        m_toolProvider->addSession(session.get());
        m_sessions.insert(config.name, session);
    }

    void McpManager::unregisterServer(const QString& name) {
        if (m_sessions.contains(name)) {
            stopServer(name);
            auto session = m_sessions.take(name);
            m_toolProvider->removeSession(session.get());
        }
    }

    bool McpManager::startServer(const QString& name) {
        if (!m_sessions.contains(name)) return false;

        auto session = m_sessions.value(name);
        if (session && session->state() == McpSessionState::Connected) {
            return true;
        }
        if (session && session->start()) {
            emit serverStarted(name);
            return true;
        }
        return false;
    }

    void McpManager::stopServer(const QString& name) {
        if (m_sessions.contains(name)) {
            auto session = m_sessions.value(name);
            if (session) {
                session->stop();
            }
            emit serverStopped(name);
        }
    }

    void McpManager::stopServersForProject(const QString& workspaceRoot) {
        if (workspaceRoot.isEmpty()) return;
        QStringList toRemove;
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value() && it.value()->config().cwd == workspaceRoot) {
                toRemove.append(it.key());
            }
        }
        for (const auto& name : toRemove) {
            unregisterServer(name);
        }
    }

    void McpManager::startAll() {
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value() && !it.value()->config().disabled) {
                if (it.value()->start()) {
                    emit serverStarted(it.key());
                }
            }
        }
    }

    void McpManager::stopAll() {
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value()) {
                it.value()->stop();
            }
            emit serverStopped(it.key());
        }
    }

    std::shared_ptr<McpToolProvider> McpManager::toolProvider() const {
        return m_toolProvider;
    }

    McpSession* McpManager::getSession(const QString& name) const {
        if (m_sessions.contains(name)) {
            return m_sessions.value(name).get();
        }
        return nullptr;
    }

    QList<McpSession*> McpManager::allSessions() const {
        QList<McpSession*> list;
        for (const auto& s : m_sessions) {
            if (s) {
                list.append(s.get());
            }
        }
        return list;
    }

} // namespace llm::mcp
