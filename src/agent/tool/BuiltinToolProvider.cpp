#include "BuiltinToolProvider.h"

#include "agent/tool/builtin/ReadFileTool.h"
#include "agent/tool/builtin/WriteFileTool.h"
#include "agent/tool/builtin/ListFilesTool.h"
#include "agent/tool/builtin/SearchTextTool.h"

namespace agent::tool {

    BuiltinToolProvider::BuiltinToolProvider(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }

        m_tools = {
            std::make_shared<builtin::ReadFileTool>(m_fs),
            std::make_shared<builtin::WriteFileTool>(m_fs),
            std::make_shared<builtin::ListFilesTool>(m_fs),
            std::make_shared<builtin::SearchTextTool>(m_fs)
        };
    }

    QList<std::shared_ptr<application::ports::ITool>> BuiltinToolProvider::tools() const {
        return m_tools;
    }

} // namespace agent::tool
