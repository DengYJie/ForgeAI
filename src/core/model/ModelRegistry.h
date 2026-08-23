#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include <optional>
#include <memory>
#include <utility>

#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"
#include "domain/repository/IModelRepository.h"

namespace core::model {

    /**
     * @brief 模型与服务商全局注册管理中心
     */
    class ModelRegistry : public QObject {
        Q_OBJECT

    public:
        explicit ModelRegistry(std::shared_ptr<domain::repository::IModelRepository> repository, QObject *parent = nullptr);
        ~ModelRegistry() override = default;

        /**
         * @brief 从本地 SQLite 仓储与打包的内置预设 JSON 文件中初始化注册中心
         * @param presetJsonPath 内置静态预设资源路径
         * @return 成功返回 true，失败返回 false
         */
        bool initialize(const QString &presetJsonPath = QStringLiteral(":/config/models.json"));

        /**
         * @brief 获取所有已激活的服务商配置列表
         */
        QList<domain::model::ModelProvider> getActiveProviders() const;

        /**
         * @brief 根据 providerId 获取指定服务商配置
         */
        std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) const;

        /**
         * @brief 保存或更新服务商配置
         */
        void saveProvider(const domain::model::ModelProvider &provider);

        /**
         * @brief 删除服务商配置
         */
        void deleteProvider(const QString &providerId);

        /**
         * @brief 获取当前所有已启用的模型列表（供 UI 顶部切换栏渲染）
         */
        QList<domain::model::Model> getEnabledModels() const;

        /**
         * @brief 核心解析函数：根据 modelId 高速解析出模型实体与其归属服务商配置
         * @param modelId 模型的唯一 ID
         * @return 匹配成功返回 std::pair<Model, ModelProvider>，未找到返回 std::nullopt
         */
        std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> resolve(const QString &modelId) const;

        /**
         * @brief 检查指定模型是否支持某项能力
         */
        bool hasCapability(const QString &modelId, domain::model::ModelCapability cap) const;

        /**
         * @brief 将发现的模型列表与本地预设模板 (models.json) 及已存在配置进行智能匹配并补充元数据
         * @param providerId 归属服务商 ID
         * @param discoveredModels 远端探测到的轻量模型实体
         * @return 经过元数据补全并保留用户配置的完整模型列表
         */
        QList<domain::model::Model> hydrateDiscoveredModels(
            const QString &providerId,
            const QList<domain::model::Model> &discoveredModels) const;

        /**
         * @brief 自动探测并注册本地 Ollama 实例中已下载的模型列表
         */
        void scanLocalOllamaModels(const QString &ollamaBaseUrl = QStringLiteral("http://localhost:11434"));

    signals:
        /**
         * @brief 服务商列表变动信号（通知设置页等 UI 刷新）
         */
        void providersChanged();

        /**
         * @brief 可用模型列表变动信号（通知主界面模型选择下拉框刷新）
         */
        void modelsChanged();

    private:
        void loadPresetJson(const QString &jsonPath);
        void rebuildModelIndex();

        std::shared_ptr<domain::repository::IModelRepository> m_repository;

        QHash<QString, domain::model::ModelProvider> m_providers; ///< providerId -> Provider
        QHash<QString, domain::model::Model> m_models;           ///< modelId -> Model
        QHash<QString, domain::model::Model> m_presetTemplates;  ///< 内置只读预设模板
    };

} // namespace core::model
