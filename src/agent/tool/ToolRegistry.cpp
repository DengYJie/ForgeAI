#include "ToolRegistry.h"
#include <QSet>

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

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_providers.contains(provider)) {
            m_providers.append(provider);
        }

        const auto toolList = provider->tools();
        for (const auto& tool : toolList) {
            if (tool) {
                const auto def = tool->definition();
                if (!def.name.isEmpty() && !m_tools.contains(def.name)) {
                    m_tools.insert(def.name, tool);
                }
            }
        }
        return toolList.size();
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
        if (m_tools.contains(name)) {
            return m_tools.value(name);
        }

        for (const auto& provider : m_providers) {
            if (!provider) continue;
            const auto toolList = provider->tools();
            for (const auto& tool : toolList) {
                if (tool && tool->definition().name == name) {
                    return tool;
                }
            }
        }
        return nullptr;
    }

    bool ToolRegistry::hasTool(const QString& name) const {
        return findTool(name) != nullptr;
    }

    QList<domain::agent::ToolDefinition> ToolRegistry::definitions() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        QList<domain::agent::ToolDefinition> defs;
        QSet<QString> seenNames;

        for (const auto& tool : m_tools) {
            if (tool) {
                const auto def = tool->definition();
                if (!seenNames.contains(def.name)) {
                    seenNames.insert(def.name);
                    defs.append(def);
                }
            }
        }

        for (const auto& provider : m_providers) {
            if (!provider) continue;
            const auto toolList = provider->tools();
            for (const auto& tool : toolList) {
                if (tool) {
                    const auto def = tool->definition();
                    if (!seenNames.contains(def.name)) {
                        seenNames.insert(def.name);
                        defs.append(def);
                    }
                }
            }
        }

        return defs;
    }

    std::unique_ptr<application::ports::IToolOperation> ToolRegistry::execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) {
        auto tool = findTool(call.name);
        if (!tool) {
            return std::make_unique<application::ports::ImmediateToolOperation>(
                call.id,
                [call]() {
                    return domain::agent::ToolResult{
                        call.id,
                        QStringLiteral("未知工具: %1").arg(call.name),
                        true
                    };
                }
            );
        }

        return tool->execute(call, context);
    }

} // namespace agent::tool
