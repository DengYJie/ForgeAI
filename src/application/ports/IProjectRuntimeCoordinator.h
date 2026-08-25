#pragma once

#include <QString>

namespace application::ports {

    /**
     * @brief 项目运行时协调器抽象接口
     * @details 负责在应用层调度项目切换与资源释放，屏蔽底层 MCP、索引与监听器等具体基础设施。
     */
    class IProjectRuntimeCoordinator {
    public:
        virtual ~IProjectRuntimeCoordinator() = default;

        /**
         * @brief 响应工作区项目切换，释放旧项目关联资源
         */
        virtual void switchProject(
            const QString& previousProjectRoot,
            const QString& newProjectRoot
        ) = 0;

        /**
         * @brief 卸载并释放指定项目的运行时资源
         */
        virtual void unloadProject(
            const QString& projectRoot
        ) = 0;
    };

} // namespace application::ports
