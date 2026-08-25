#include "McpConfigLoader.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::mcp {

    McpConfigLoadResult McpConfigLoader::loadFromFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.exists()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpConfig, QStringLiteral("MCP 配置文件不存在"), {
                {QStringLiteral("filePath"), filePath}
            });
            return {false, QStringLiteral("配置文件不存在: %1").arg(filePath), {}, {}};
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpConfig, QStringLiteral("无法读取 MCP 配置文件"), {
                {QStringLiteral("filePath"), filePath},
                {QStringLiteral("osError"), file.errorString()}
            });
            return {false, QStringLiteral("无法读取配置文件: %1 (原因: %2)").arg(filePath, file.errorString()), {}, {}};
        }

        const QByteArray content = file.readAll();
        file.close();

        const QString baseDir = QFileInfo(filePath).absolutePath();
        return loadFromJson(content, baseDir);
    }

    McpConfigLoadResult McpConfigLoader::loadFromJsonString(const QString& jsonString, const QString& baseDir) {
        return loadFromJson(jsonString.toUtf8(), baseDir);
    }

    McpConfigLoadResult McpConfigLoader::loadFromJson(const QByteArray& jsonData, const QString& baseDir) {
        McpConfigLoadResult result;
        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(jsonData, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            core::logging::LoggingService::instance().warn(core::logging::Category::McpConfig, QStringLiteral("MCP 配置文件 JSON 解析失败"), {
                {QStringLiteral("error"), parseErr.errorString()},
                {QStringLiteral("offset"), QString::number(parseErr.offset)}
            });
            result.success = false;
            result.error = QStringLiteral("JSON 语法解析错误: %1 (offset: %2)").arg(parseErr.errorString()).arg(parseErr.offset);
            return result;
        }

        if (!doc.isObject()) {
            result.success = false;
            result.error = QStringLiteral("MCP 配置文件根节点必须为 JSON 对象");
            return result;
        }

        const auto rootObj = doc.object();
        QJsonObject serversObj;
        if (rootObj.contains(QStringLiteral("mcpServers")) && rootObj.value(QStringLiteral("mcpServers")).isObject()) {
            serversObj = rootObj.value(QStringLiteral("mcpServers")).toObject();
        } else {
            serversObj = rootObj;
        }

        for (auto it = serversObj.begin(); it != serversObj.end(); ++it) {
            const QString serverKey = it.key();
            if (!it.value().isObject()) {
                result.issues.append({serverKey, QStringLiteral("服务器项必须为 JSON 对象"), true});
                continue;
            }

            const auto serverObj = it.value().toObject();
            domain::mcp::McpServerConfig cfg;

            cfg.id = serverObj.value(QStringLiteral("id")).toString(serverKey);
            cfg.name = serverObj.value(QStringLiteral("name")).toString(cfg.id);

            const QString transportStr = serverObj.value(QStringLiteral("transport")).toString().toLower();
            if (transportStr == QStringLiteral("http") || transportStr == QStringLiteral("sse")) {
                cfg.transport = domain::mcp::McpTransportType::Http;
            } else if (transportStr == QStringLiteral("websocket") || transportStr == QStringLiteral("ws")) {
                cfg.transport = domain::mcp::McpTransportType::WebSocket;
            } else if (transportStr == QStringLiteral("stdio")) {
                cfg.transport = domain::mcp::McpTransportType::Stdio;
            } else {
                if (serverObj.contains(QStringLiteral("url"))) {
                    cfg.transport = domain::mcp::McpTransportType::Http;
                } else {
                    cfg.transport = domain::mcp::McpTransportType::Stdio;
                }
            }

            cfg.command = serverObj.value(QStringLiteral("command")).toString();
            const auto argsArr = serverObj.value(QStringLiteral("args")).toArray();
            for (const auto& a : argsArr) {
                cfg.args.append(a.toString());
            }

            const auto envObj = serverObj.value(QStringLiteral("env")).toObject();
            for (auto envIt = envObj.begin(); envIt != envObj.end(); ++envIt) {
                cfg.env.insert(envIt.key(), envIt.value().toString());
            }

            QString rawCwd = serverObj.value(QStringLiteral("cwd")).toString();
            if (!rawCwd.isEmpty() && !baseDir.isEmpty()) {
                QDir dir(baseDir);
                if (QDir::isRelativePath(rawCwd)) {
                    cfg.cwd = dir.absoluteFilePath(rawCwd);
                } else {
                    cfg.cwd = rawCwd;
                }
            } else {
                cfg.cwd = rawCwd;
            }

            cfg.url = serverObj.value(QStringLiteral("url")).toString();
            const auto headersObj = serverObj.value(QStringLiteral("headers")).toObject();
            for (auto hIt = headersObj.begin(); hIt != headersObj.end(); ++hIt) {
                cfg.headers.insert(hIt.key(), hIt.value().toString());
            }

            if (serverObj.contains(QStringLiteral("enabled"))) {
                cfg.enabled = serverObj.value(QStringLiteral("enabled")).toBool(true);
                cfg.disabled = !cfg.enabled;
            } else if (serverObj.contains(QStringLiteral("disabled"))) {
                cfg.disabled = serverObj.value(QStringLiteral("disabled")).toBool(false);
                cfg.enabled = !cfg.disabled;
            }
            cfg.autoApprove = serverObj.value(QStringLiteral("autoApprove")).toBool(false);

            if (cfg.id.trimmed().isEmpty()) {
                result.issues.append({serverKey, QStringLiteral("MCP 服务 ID 不能为空"), true});
                continue;
            }

            if (cfg.transport == domain::mcp::McpTransportType::Stdio && cfg.command.trimmed().isEmpty()) {
                result.issues.append({cfg.id, QStringLiteral("Stdio 传输协议缺少 command 启动命令"), true});
                continue;
            }

            if (cfg.transport == domain::mcp::McpTransportType::Http && cfg.url.trimmed().isEmpty()) {
                result.issues.append({cfg.id, QStringLiteral("HTTP 传输协议缺少 url 目标地址"), true});
                continue;
            }

            result.configs.append(cfg);
        }

        if (result.configs.isEmpty() && !result.issues.isEmpty()) {
            result.success = false;
            QStringList msgs;
            for (const auto& issue : result.issues) {
                msgs.append(QStringLiteral("[%1] %2").arg(issue.serverId, issue.message));
            }
            result.error = QStringLiteral("所有 MCP 服务配置项均校验失败: %1").arg(msgs.join(QStringLiteral("; ")));
        }

        core::logging::LoggingService::instance().info(core::logging::Category::McpConfig, QStringLiteral("MCP 配置加载完成"), {
            {QStringLiteral("configsCount"), QString::number(result.configs.size())},
            {QStringLiteral("issuesCount"), QString::number(result.issues.size())},
            {QStringLiteral("success"), result.success ? QStringLiteral("true") : QStringLiteral("false")}
        });

        return result;
    }

} // namespace llm::mcp
