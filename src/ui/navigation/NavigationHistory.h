#ifndef OMNIAUTH_UI_NAVIGATION_NAVIGATIONHISTORY_H
#define OMNIAUTH_UI_NAVIGATION_NAVIGATIONHISTORY_H

#include <QObject>
#include <QStringList>

namespace ui::navigation {
    /**
     * @brief 导航历史栈管理器
     *
     * 负责记录界面路由栈的跳转历史，支持回退（Back）与前进（Forward）操作。
     * 其设计原则与 WinUI 3 保持一致，通过管理独立的状态栈来为 TitleBar 返回按钮提供数据支撑。
     */
    class NavigationHistory : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)

        Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)

        Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)

    public:
        /**
         * @brief 构造函数
         * @param parent 父对象
         */
        explicit NavigationHistory(QObject *parent = nullptr);

        /**
         * @brief 查询当前历史记录中是否存在可供回退的旧页面
         * @return 存在后退历史返回 true，否则返回 false
         */
        bool canGoBack() const;

        /**
         * @brief 查询当前历史记录中是否存在可供前进的撤销页面
         * @return 存在前进历史返回 true，否则返回 false
         */
        bool canGoForward() const;

        /**
         * @brief 获取当前停留的页面路由键
         * @return 当前路由标识字符串
         */
        QString currentRoute() const;

        /**
         * @brief 获取完整的返回栈记录
         * @return 返回栈路由列表，末尾（top）为最近一次访问的路由
         */
        const QStringList &backStack() const;

        /**
         * @brief 获取完整的前进栈记录
         * @return 前进栈路由列表，末尾（top）为最近一次回退撤销的路由
         */
        const QStringList &forwardStack() const;

        /**
         * @brief 预览上一个路由（不改变历史状态）
         * @return 上一个路由键，栈空时返回空字符串
         */
        QString peekBack() const;

        /**
         * @brief 提交后退操作
         * @return 后退到的目标路由键
         */
        QString commitBack();

        /**
         * @brief 预览下一个路由（不改变历史状态）
         * @return 下一个路由键，栈空时返回空字符串
         */
        QString peekForward() const;

        /**
         * @brief 提交前进操作
         * @return 前进到的目标路由键
         */
        QString commitForward();

    public
        Q_SLOTS:
        /**
         * @brief 推送一个新路由，并使其成为当前活动路由
         *
         * 会将先前的当前路由压入返回栈，同时清空原有的前进栈。
         * 若新路由为空或与当前路由一致，则直接忽略。
         *
         * @param newRoute 要跳转的新路由键
         */




        void push(const QString &newRoute);

        /**
         * @brief 替换当前路由（不入栈旧路由，不清除前进栈）
         * @param newRoute 要替换的新路由键
         */
        void replace(const QString &newRoute);

        /**
         * @brief 弹栈并执行后退逻辑
         *
         * 将当前活动路由压入前进栈，并弹出返回栈最顶部的旧路由作为新路由返回。
         *
         * @return 后退到的旧路由标识；如果无法后退，返回空字符串
         */
        QString goBack();

        /**
         * @brief 弹栈并执行前进逻辑
         *
         * 将当前活动路由压入后退栈，并弹出前进栈最顶部的路由作为新路由返回。
         *
         * @return 前进到的路由标识；如果无法前进，返回空字符串
         */
        QString goForward();

        /**
         * @brief 清空所有历史导航记录栈以及当前路由状态
         */
        void clear();

        Q_SIGNALS:
        /**
         * @brief 后退可用状态发生变化时触发
         * @param canGoBack 当前是否允许后退
         */




        void canGoBackChanged(bool canGoBack);

        /**
         * @brief 前进可用状态发生变化时触发
         * @param canGoForward 当前是否允许前进
         */
        void canGoForwardChanged(bool canGoForward);

        /**
         * @brief 当前聚焦的活动路由发生变化时触发
         * @param currentRoute 新的当前路由键
         */
        void currentRouteChanged(const QString &currentRoute);

        /**
         * @brief 执行了真正的页面后退操作后触发
         * @param routeKey 后退退回到的目标路由
         */
        void navigatedBack(const QString &routeKey);

        /**
         * @brief 执行了真正的页面前进操作后触发
         * @param routeKey 前进进入的目标路由
         */
        void navigatedForward(const QString &routeKey);

    private:
        QString m_currentRoute;
        QStringList m_backStack;
        QStringList m_forwardStack;
    };
} // namespace ui::navigation

#endif // OMNIAUTH_UI_NAVIGATION_NAVIGATIONHISTORY_H
