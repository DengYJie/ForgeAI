#include "ProcessLaunchResolver.h"

namespace services::process {

    ProcessLaunchSpec ProcessLaunchResolver::resolveShellCommand(
        const domain::agent::task::ProcessTaskSpec& task,
        const domain::process::ShellProfile& shell
    ) const {
        ProcessLaunchSpec spec;
        spec.executable = shell.executable;

        // 1. 组装启动参数
        spec.arguments = shell.startupArguments;

        // 2. 组装命令开关与命令行字符串
        if (!shell.commandArgument.isEmpty()) {
            spec.arguments.append(shell.commandArgument);
        }
        spec.arguments.append(task.command);

        // 3. 环境变量注入
        if (!shell.environment.isEmpty()) {
            spec.environment = QProcessEnvironment::systemEnvironment();
            for (auto it = shell.environment.constBegin(); it != shell.environment.constEnd(); ++it) {
                spec.environment.insert(it.key(), it.value());
            }
        }

        return spec;
    }

    ProcessLaunchSpec ProcessLaunchResolver::resolveDirectProcess(
        const domain::agent::task::ProcessTaskSpec& task
    ) const {
        ProcessLaunchSpec spec;
        spec.executable = task.program;
        spec.arguments = task.arguments;
        return spec;
    }

} // namespace services::process
