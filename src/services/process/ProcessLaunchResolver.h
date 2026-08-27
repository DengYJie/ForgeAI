#pragma once

#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include "domain/process/ShellProfile.h"
#include "domain/agent/task/ProcessTaskSpec.h"

namespace services::process {

    /**
     * @brief 进程启动解析后的最终目标参数规格
     */
    struct ProcessLaunchSpec {
        QString executable;
        QStringList arguments;
        QProcessEnvironment environment;
    };

    /**
     * @brief 进程启动参数解析器
     */
    class ProcessLaunchResolver {
    public:
        /**
         * @brief 将 ShellCommand 模式的任务规格与 Shell 配置解析为最终的可执行程序及参数
         */
        ProcessLaunchSpec resolveShellCommand(
            const domain::agent::task::ProcessTaskSpec& task,
            const domain::process::ShellProfile& shell
        ) const;

        /**
         * @brief 将 DirectProcess 模式的任务规格解析为最终参数
         */
        ProcessLaunchSpec resolveDirectProcess(
            const domain::agent::task::ProcessTaskSpec& task
        ) const;
    };

} // namespace services::process
