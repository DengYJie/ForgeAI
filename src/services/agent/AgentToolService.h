#pragma once

#include "domain/service/IAgentToolService.h"

namespace services::agent {
class AgentToolService final : public domain::service::IAgentToolService {
    Q_OBJECT
public:
    explicit AgentToolService(QString workspaceRoot, QObject* parent = nullptr);
    void setWorkspaceRoot(const QString& workspaceRoot);
    QList<domain::agent::ToolDefinition> definitions() const override;
    domain::agent::ToolResult execute(const domain::agent::ToolCall& call) override;
private:
    QString resolveWorkspacePath(const QString& relativePath, QString* error) const;
    QString resolveWritableWorkspacePath(const QString& relativePath, QString* error) const;
    QString m_workspaceRoot;
};
} // namespace services::agent
