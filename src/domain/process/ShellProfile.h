#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace domain::process {

    /**
     * @brief 终端 Shell 类型枚举
     */
    enum class ShellType {
        PowerShell,     ///< PowerShell 7 (pwsh) 或 Windows PowerShell (powershell)
        Cmd,            ///< Windows Command Prompt (cmd.exe)
        Bash,           ///< Bourne-Again SHell (bash / Git Bash)
        Zsh,            ///< Z Shell (zsh)
        Fish,           ///< Friendly Interactive Shell (fish)
        Wsl,            ///< Windows Subsystem for Linux (wsl.exe)
        Custom          ///< 用户自定义 Shell
    };

    /**
     * @brief 终端 Shell 配置描述
     */
    struct ShellProfile {
        QString id;                                 ///< 唯一标识 (如 "powershell-7", "cmd", "bash")
        QString name;                               ///< 显示名称 (如 "PowerShell 7", "Command Prompt")
        ShellType type = ShellType::Custom;         ///< Shell 类型分类

        QString executable;                         ///< Shell 可执行文件路径或名称 (如 "pwsh.exe", "/bin/bash")
        QStringList startupArguments;               ///< 启动参数列表 (如 ["-NoLogo", "-NoProfile"] 或 ["/D", "/S"])
        QString commandArgument;                    ///< 执行单条命令的开关参数 (如 "-Command", "-c", "/C")

        QMap<QString, QString> environment;         ///< 附加环境变量映射
        bool enabled = true;                        ///< 是否启用

        bool operator==(const ShellProfile& other) const {
            return id == other.id &&
                   name == other.name &&
                   type == other.type &&
                   executable == other.executable &&
                   startupArguments == other.startupArguments &&
                   commandArgument == other.commandArgument &&
                   environment == other.environment &&
                   enabled == other.enabled;
        }
    };

} // namespace domain::process
