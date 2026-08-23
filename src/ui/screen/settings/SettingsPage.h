#pragma once

#include <QList>
#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::settings {
    class SettingsCardItem;
    class SettingsViewModel;
    class SettingsUIRegistry;
    class SettingsCoordinator;
    struct SettingsState;

    /**
     * @brief 设置主界面视图 (通用渲染容器)
     * @details 作为纯粹的 View 层渲染容器，不直接依赖任何具体设置业务，通过遍历 SettingsUIRegistry 中的工厂动态构建分区与卡片
     */
    class SettingsPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        /**
         * @param viewModel 页面级 ViewModel
         * @param uiRegistry 设置 UI 工厂注册中心
         * @param coordinator 表现层协调者
         * @param parent 宿主父控件
         */
        explicit SettingsPage(
            SettingsViewModel *viewModel = nullptr,
            SettingsUIRegistry *uiRegistry = nullptr,
            SettingsCoordinator *coordinator = nullptr,
            QWidget *parent = nullptr
        );

        ~SettingsPage() override = default;

        /**
         * @brief 获取关联的页面级 ViewModel
         */
        SettingsViewModel *viewModel() const { return m_viewModel; }

        /**
         * @brief 获取关联的表现层协调者
         */
        SettingsCoordinator *coordinator() const { return m_coordinator; }

        /**
         * @brief 主题更新回调
         */
        void onThemeUpdated() override;

    protected:
        /**
         * @brief 响应式宽度布局刷新
         * @param availableWidth 当前可视可用宽度
         */
        void updateResponsiveLayout(int availableWidth) override;

    private:
        void setupUi();
        void render(const SettingsState &state);
        QWidget *createSectionHeader(const QString &title);
        QWidget *createSettingsCard(const QString &iconGlyph,
                                    const QString &title,
                                    const QString &subtitle,
                                    QWidget *trailingWidget);

        SettingsViewModel *m_viewModel = nullptr;
        SettingsUIRegistry *m_uiRegistry = nullptr;
        SettingsCoordinator *m_coordinator = nullptr;

        QWidget *m_viewport = nullptr;
        QVBoxLayout *m_contentLayout = nullptr;
        fluent::textfields::Label *m_titleLabel = nullptr;
        QList<SettingsCardItem *> m_cards;
    };
} // namespace ui::screen::settings
