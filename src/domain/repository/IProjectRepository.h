#pragma once
#include <QList>
#include <QUuid>
#include <optional>
#include "domain/project/Project.h"

namespace domain::repository {

    /**
     * @brief 项目/工作区仓储接口
     */
    class IProjectRepository {
    public:
        virtual ~IProjectRepository() = default;

        /**
         * @brief 获取所有已注册的项目列表（按最近打开时间倒序）
         */
        virtual QList<domain::project::Project> getAllProjects() = 0;

        /**
         * @brief 根据 UUID 获取项目详情
         */
        virtual std::optional<domain::project::Project> getProject(const QUuid &id) = 0;

        /**
         * @brief 根据本地磁盘路径获取已存在的项目
         */
        virtual std::optional<domain::project::Project> getProjectByPath(const QString &rootPath) = 0;

        /**
         * @brief 保存或更新项目信息
         */
        virtual void saveProject(const domain::project::Project &project) = 0;

        /**
         * @brief 删除项目记录（不删除物理磁盘文件）
         */
        virtual void deleteProject(const QUuid &id) = 0;
    };

} // namespace domain::repository
