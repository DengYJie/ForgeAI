#pragma once
#include <QDialog>
#include <QPointer>
#include <FluentQt/FluentQt.h>
#include "domain/model/ModelProvider.h"
#include "ProviderNavigationPane.h"
#include "ProviderDetailView.h"

namespace core::model {
    class ModelRegistry;
}
namespace application::usecase::settings {
    class RefreshModelsUseCase;
}

namespace ui::screen::settings::model_manager {

    /**
     * @brief 模型与服务商全功能管理对话框
     * @details Master-Detail 架构设计，左侧导航栏承载服务商检索与切换，右侧工作区管理端点、密钥与模型列表
     */
    class ModelManagerDialog : public QDialog, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ModelManagerDialog(
            core::model::ModelRegistry *registry = nullptr,
            application::usecase::settings::RefreshModelsUseCase *refreshUseCase = nullptr,
            QWidget *parent = nullptr
        );
        ~ModelManagerDialog() override = default;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void loadProviders();
        void onAddProvider();
        void onAddModel(const QString &providerId);

        core::model::ModelRegistry *m_registry = nullptr;
        application::usecase::settings::RefreshModelsUseCase *m_refreshUseCase = nullptr;

        ProviderNavigationPane *m_navPane = nullptr;
        ProviderDetailView *m_detailView = nullptr;
    };

} // namespace ui::screen::settings::model_manager
