#pragma once

#include <QList>
#include <QString>
#include <optional>
#include "domain/process/ShellProfile.h"

namespace application::ports {

    /**
     * @brief 终端 Shell 服务接口
     */
    class IShellService {
    public:
        virtual ~IShellService() = default;

        /**
         * @brief 获取系统当前已发现并可用的全部 Shell 列表
         */
        virtual QList<domain::process::ShellProfile> availableShells() const = 0;

        /**
         * @brief 根据 ID 获取指定 Shell 配置
         */
        virtual std::optional<domain::process::ShellProfile> shell(const QString& id) const = 0;

        /**
         * @brief 获取当前系统默认选中的 Shell
         */
        virtual std::optional<domain::process::ShellProfile> defaultShell() const = 0;

        /**
         * @brief 设置默认 Shell ID
         */
        virtual bool setDefaultShell(const QString& id) = 0;

        /**
         * @brief 重新扫描探测系统上安装的 Shell
         */
        virtual void refreshAvailableShells() = 0;
    };

} // namespace application::ports
