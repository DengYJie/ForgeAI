#include "ToolRegistry.h"

namespace agent::tool {

    bool ToolRegistry::registerTool(std::shared_ptr<application::ports::ITool> tool) {
        if (!tool) return false;

        const auto def = tool->definition();
        if (def.name.trimmed().isEmpty()) return false;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tools.contains(def.name)) {
            return false; // 重名检测防御
        }

        m_tools.insert(def.name, tool);
        return true;
    }

    int ToolRegistry::registerProvider(std::shared_ptr<application::ports::IToolProvider> provider) {
        if (!provider) return 0;

        int count = 0;
        const auto toolList = provider->tools();
        for (const auto& tool : toolList) {
            if (registerTool(tool)) {
                ++count;
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_providers.append(provider);
        return count;
    }

    void ToolRegistry::unregisterTool(const QString& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tools.remove(name);
    }

    void ToolRegistry::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tools.clear();
        m_providers.clear();
    }

    std::shared_ptr<application::ports::ITool> ToolRegistry::findTool(const QString& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tools.value(name, nullptr);
    }

    bool ToolRegistry::hasTool(const QString& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tools.contains(name);
    }

    QList<domain::agent::ToolDefinition> ToolRegistry::definitions() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::agent::ToolDefinition> defs;
        defs.reserve(m_tools.size());
        for (const auto& tool : m_tools) {
            if (tool) {
                defs.append(tool->definition());
            }
        }
        return defs;
    }

    domain::agent::ToolResult ToolRegistry::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        std::shared_ptr<application::ports::ITool> tool;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            tool = m_tools.value(call.name, nullptr);
        }

        if (!tool) {
            return domain::agent::ToolResult{
                call.id,
                QStringLiteral("未知工具: %1").arg(call.name),
                true
            };
        }

        return tool->execute(call, context);
    }

} // namespace agent::tool
