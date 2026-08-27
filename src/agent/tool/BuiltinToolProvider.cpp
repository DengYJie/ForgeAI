#include "BuiltinToolProvider.h"

#include "agent/task/ProcessTaskRuntime.h"
#include "services/process/ShellService.h"
#include "agent/tool/builtin/ReadFileTool.h"
#include "agent/tool/builtin/WriteFileTool.h"
#include "agent/tool/builtin/ListFilesTool.h"
#include "agent/tool/builtin/SearchTextTool.h"
#include "agent/tool/builtin/ApplyPatchTool.h"
#include "agent/tool/builtin/RunCommandTool.h"
#include "agent/tool/builtin/CheckTaskTool.h"

namespace agent::tool {

    BuiltinToolProvider::BuiltinToolProvider()
        : BuiltinToolProvider(nullptr, nullptr, nullptr) {
    }

    BuiltinToolProvider::BuiltinToolProvider(std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs)
        : BuiltinToolProvider(nullptr, nullptr, std::move(fs)) {
    }

    BuiltinToolProvider::BuiltinToolProvider(
        std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
        std::shared_ptr<application::ports::IShellService> shellService,
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs
    ) : m_taskRuntime(std::move(taskRuntime)),
        m_shellService(std::move(shellService)),
        m_fs(std::move(fs)) {
        if (!m_fs) {
            m_fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
        }
        if (!m_shellService) {
            m_shellService = std::make_shared<services::process::ShellService>();
        }
        if (!m_taskRuntime) {
            m_taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>(m_shellService);
        }

        m_tools = {
            std::make_shared<builtin::ReadFileTool>(m_fs),
            std::make_shared<builtin::WriteFileTool>(m_fs),
            std::make_shared<builtin::ListFilesTool>(m_fs),
            std::make_shared<builtin::SearchTextTool>(m_fs),
            std::make_shared<builtin::ApplyPatchTool>(m_fs),
            std::make_shared<builtin::RunCommandTool>(m_taskRuntime, m_shellService, m_fs),
            std::make_shared<builtin::CheckTaskTool>(m_taskRuntime)
        };
    }

    QList<std::shared_ptr<application::ports::ITool>> BuiltinToolProvider::tools() const {
        return m_tools;
    }

} // namespace agent::tool
