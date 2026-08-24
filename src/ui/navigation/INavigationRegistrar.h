#pragma once

#include <QString>
#include <memory>
#include "ui/navigation/NavigationMetrics.h"

class QWidget;

namespace ui::animation {
    class AnimatedVisualSource;
}

namespace ui::navigation {
    class NavigationPanel;

    /**
     * @brief 导航项描述符
     */
    struct NavigationItemDescriptor {
        QString routeKey;
        QString iconGlyph;
        QString text;
        QString parentRouteKey;
        NavigationItemPosition position = NavigationItemPosition::Top;
        bool selectable = true;
        QString tooltip;
        std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource = nullptr;
    };

    /**
     * @brief 导航注册器接口
     * @details 供各业务特性模块（Feature Modules）安装 Surface、注册路由与添加导航项
     */
    class INavigationRegistrar {
    public:
        virtual ~INavigationRegistrar() = default;

        /**
         * @brief 注册导航 Surface 面板
         * @param surfaceId Surface 唯一标识
         * @param panel 导航面板实例（注册成功后由宿主容器管理）
         * @return 注册是否成功
         */
        virtual bool registerSurface(
            const QString &surfaceId,
            ui::navigation::NavigationPanel *panel
        ) = 0;

        /**
         * @brief 注册路由与对应的内容页控件
         * @param routeKey 路由唯一键
         * @param page 内容页控件
         * @param surfaceId 宿主 Surface 标识
         * @return 注册是否成功
         */
        virtual bool registerRoute(
            const QString &routeKey,
            QWidget *page,
            const QString &surfaceId = QStringLiteral("main")
        ) = 0;

        /**
         * @brief 向指定 Surface 添加导航项
         * @param item 导航项描述符
         * @param surfaceId 宿主 Surface 标识
         * @return 添加是否成功
         */
        virtual bool addNavigationItem(
            const NavigationItemDescriptor &item,
            const QString &surfaceId = QStringLiteral("main")
        ) = 0;
    };

} // namespace ui::navigation
