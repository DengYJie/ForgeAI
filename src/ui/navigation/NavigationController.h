#pragma once

#include <QObject>
#include <QString>

#include "ui/navigation/INavigationPresenter.h"
#include "ui/navigation/NavigationTypes.h"

namespace ui::navigation {
    class NavigationHistory;

    /**
     * @brief 导航控制器（整个应用导航行为的唯一决策入口）
     * @details 集中持有 NavigationHistory，具有严格的渲染事务原子性与防重入保障：仅在 INavigationPresenter 渲染成功后才提交 History 变更
     */
    class NavigationController : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
        Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)
        Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)

    public:
        explicit NavigationController(INavigationPresenter *presenter, QObject *parent = nullptr);
        ~NavigationController() override;

        INavigationPresenter *presenter() const { return m_presenter; }
        void setPresenter(INavigationPresenter *presenter);

        bool canGoBack() const;
        bool canGoForward() const;
        QString currentRoute() const;

        const NavigationHistory *history() const { return m_history; }

    public Q_SLOTS:
        /**
         * @brief 正常推进导航（验证并驱动渲染成功后，压入历史栈）
         * @param routeKey 目标路由键
         * @return 导航是否成功
         */
        bool navigate(const QString &routeKey);

        /**
         * @brief 执行后退导航（验证并驱动渲染成功后，弹出历史栈）
         * @return 后退是否成功
         */
        bool goBack();

        /**
         * @brief 执行前进导航（验证并驱动渲染成功后，推进历史栈）
         * @return 前进是否成功
         */
        bool goForward();

        /**
         * @brief 替换当前页面（验证并驱动渲染成功后，替换当前历史）
         * @param routeKey 目标路由键
         * @return 替换是否成功
         */
        bool replace(const QString &routeKey);

    Q_SIGNALS:
        void canGoBackChanged(bool canGoBack);
        void canGoForwardChanged(bool canGoForward);
        void currentRouteChanged(const QString &currentRoute);

    private:
        INavigationPresenter *m_presenter = nullptr;
        NavigationHistory *m_history = nullptr;
        bool m_isNavigating = false;
    };
} // namespace ui::navigation
