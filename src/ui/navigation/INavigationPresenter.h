#pragma once

#include <QString>
#include "ui/navigation/NavigationTypes.h"

namespace ui::navigation {

    /**
     * @brief 路由呈现接口
     * @details 抽象底层 UI 容器的页面与 Surface 呈现行为，供 NavigationController 调度
     */
    class INavigationPresenter {
    public:
        virtual ~INavigationPresenter() = default;

        /**
         * @brief 查询目标路由是否已注册且宿主 Surface 可供呈现
         * @param routeKey 目标路由键
         * @return 是否可呈现
         */
        virtual bool canPresentRoute(const QString &routeKey) const = 0;

        /**
         * @brief 执行目标路由的页面切换与视觉呈现
         * @param routeKey 目标路由键
         * @param direction 导航方向
         * @return 呈现是否成功
         */
        virtual bool presentRoute(
            const QString &routeKey,
            NavigationDirection direction = NavigationDirection::Forward
        ) = 0;
    };

} // namespace ui::navigation
