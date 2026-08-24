#pragma once

#include <QString>
#include <QWidget>

namespace ui::navigation {

    /**
     * @brief 显式导航方向
     */
    enum class NavigationDirection {
        Forward = 1,
        Back = -1,
        None = 0
    };

    /**
     * @brief 路由描述符
     */
    struct RouteDescriptor {
        QString routeKey;
        QWidget* interfaceWidget = nullptr;
        QString surfaceId = QStringLiteral("main");
        int contentIndex = -1;
    };

} // namespace ui::navigation
