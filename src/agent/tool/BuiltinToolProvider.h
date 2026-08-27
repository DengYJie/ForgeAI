#pragma once

#include <memory>
#include "application/ports/IToolProvider.h"
#include "application/ports/IProcessTaskRuntime.h"
#include "application/ports/IShellService.h"
#include "llm/workspace/WorkspaceFileSystem.h"

namespace agent::tool {

    /**
     * @brief 内置工具提供者（提供 read_file, write_file, list_files, search_text, apply_patch, run_command, check_task）
     */
    class BuiltinToolProvider final : public application::ports::IToolProvider {
    public:
        BuiltinToolProvider();

        explicit BuiltinToolProvider(
            std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs
        );

        explicit BuiltinToolProvider(
            std::shared_ptr<application::ports::IProcessTaskRuntime> taskRuntime,
            std::shared_ptr<application::ports::IShellService> shellService = nullptr,
            std::shared_ptr<llm::workspace::WorkspaceFileSystem> fs = nullptr
        );
        ~BuiltinToolProvider() override = default;

        QList<std::shared_ptr<application::ports::ITool>> tools() const override;

    private:
        std::shared_ptr<application::ports::IProcessTaskRuntime> m_taskRuntime;
        std::shared_ptr<application::ports::IShellService> m_shellService;
        std::shared_ptr<llm::workspace::WorkspaceFileSystem> m_fs;
        QList<std::shared_ptr<application::ports::ITool>> m_tools;
    };

} // namespace agent::tool
