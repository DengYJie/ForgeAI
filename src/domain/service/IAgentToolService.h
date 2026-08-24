#pragma once

#include <QObject>
#include <QList>
#include "domain/agent/ToolDefinition.h"
#include "domain/agent/ToolExecution.h"

namespace domain::service {
class IAgentToolService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IAgentToolService() override = default;
    virtual QList<domain::agent::ToolDefinition> definitions() const = 0;
    virtual domain::agent::ToolResult execute(const domain::agent::ToolCall& call) = 0;
};
} // namespace domain::service
