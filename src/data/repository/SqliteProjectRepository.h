#pragma once
#include <QString>
#include "domain/repository/IProjectRepository.h"

namespace data::repository {
    /**
     * @brief 基于 SQLite 的项目元数据仓储实现
     */
    class SqliteProjectRepository : public domain::repository::IProjectRepository {
    public:
        explicit SqliteProjectRepository(const QString &connectionName = "forgeai_db");

        ~SqliteProjectRepository() override = default;

        /**
         * @brief 初始化项目表结构
         */
        bool initializeDatabase();

        QList<domain::project::Project> getAllProjects() override;

        std::optional<domain::project::Project> getProject(const QUuid &id) override;

        std::optional<domain::project::Project> getProjectByPath(const QString &rootPath) override;

        void saveProject(const domain::project::Project &project) override;

        void deleteProject(const QUuid &id) override;

    private:
        QString m_connectionName;
    };
} // namespace data::repository
