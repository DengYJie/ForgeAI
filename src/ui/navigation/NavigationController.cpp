#include "ui/navigation/NavigationController.h"

#include "ui/navigation/NavigationHistory.h"
#include <QDebug>

namespace ui::navigation {

    namespace {
        struct ReentrancyGuard {
            bool &flag;
            bool active = false;

            explicit ReentrancyGuard(bool &f) : flag(f) {
                if (!flag) {
                    flag = true;
                    active = true;
                }
            }

            ~ReentrancyGuard() {
                if (active) {
                    flag = false;
                }
            }
        };
    } // namespace

    NavigationController::NavigationController(INavigationPresenter *presenter, QObject *parent)
        : QObject(parent)
        , m_presenter(presenter)
        , m_history(new NavigationHistory(this)) {

        connect(m_history, &NavigationHistory::canGoBackChanged,
                this, &NavigationController::canGoBackChanged);
        connect(m_history, &NavigationHistory::canGoForwardChanged,
                this, &NavigationController::canGoForwardChanged);
        connect(m_history, &NavigationHistory::currentRouteChanged,
                this, &NavigationController::currentRouteChanged);
    }

    NavigationController::~NavigationController() = default;

    void NavigationController::setPresenter(INavigationPresenter *presenter) {
        m_presenter = presenter;
    }

    bool NavigationController::canGoBack() const {
        return m_history && m_history->canGoBack();
    }

    bool NavigationController::canGoForward() const {
        return m_history && m_history->canGoForward();
    }

    QString NavigationController::currentRoute() const {
        return m_history ? m_history->currentRoute() : QString();
    }

    bool NavigationController::navigate(const QString &routeKey) {
        if (routeKey.isEmpty() || !m_presenter) return false;
        if (currentRoute() == routeKey) return true; // 幂等成功

        if (m_isNavigating) {
            qWarning().noquote() << "[NavigationController] navigate REJECTED: reentrant call during active transition";
            return false;
        }

        ReentrancyGuard guard(m_isNavigating);

        // 1. 先验证并执行 UI 呈现
        const bool ok = m_presenter->presentRoute(routeKey, NavigationDirection::Forward);
        if (!ok) {
            qWarning().noquote() << "[NavigationController] navigate FAILED: presenter rejected route:" << routeKey;
            return false; // 严禁污染 History
        }

        // 2. UI 呈现成功后才提交 History 变更
        if (m_history) {
            m_history->push(routeKey);
        }
        qDebug().noquote() << "[NavigationController] navigate SUCCESS to:" << routeKey;
        return true;
    }

    bool NavigationController::goBack() {
        if (!canGoBack() || !m_presenter || !m_history) {
            qDebug().noquote() << "[NavigationController] goBack skipped (stack empty or invalid presenter)";
            return false;
        }

        if (m_isNavigating) {
            qWarning().noquote() << "[NavigationController] goBack REJECTED: reentrant call during active transition";
            return false;
        }

        const QString prevRoute = m_history->peekBack();
        if (prevRoute.isEmpty()) return false;

        ReentrancyGuard guard(m_isNavigating);

        // 1. 先验证并执行 UI 呈现
        const bool ok = m_presenter->presentRoute(prevRoute, NavigationDirection::Back);
        if (!ok) {
            qWarning().noquote() << "[NavigationController] goBack FAILED: presenter rejected route:" << prevRoute;
            return false; // 渲染失败，History 保持原状
        }

        // 2. 呈现成功后才提交出栈
        m_history->commitBack();
        qDebug().noquote() << "[NavigationController] goBack SUCCESS to:" << prevRoute;
        return true;
    }

    bool NavigationController::goForward() {
        if (!canGoForward() || !m_presenter || !m_history) return false;

        if (m_isNavigating) {
            qWarning().noquote() << "[NavigationController] goForward REJECTED: reentrant call during active transition";
            return false;
        }

        const QString nextRoute = m_history->peekForward();
        if (nextRoute.isEmpty()) return false;

        ReentrancyGuard guard(m_isNavigating);

        // 1. 先验证并执行 UI 呈现
        const bool ok = m_presenter->presentRoute(nextRoute, NavigationDirection::Forward);
        if (!ok) {
            qWarning().noquote() << "[NavigationController] goForward FAILED: presenter rejected route:" << nextRoute;
            return false;
        }

        // 2. 呈现成功后才提交进栈
        m_history->commitForward();
        qDebug().noquote() << "[NavigationController] goForward SUCCESS to:" << nextRoute;
        return true;
    }

    bool NavigationController::replace(const QString &routeKey) {
        if (routeKey.isEmpty() || !m_presenter) return false;
        if (currentRoute() == routeKey) return true;

        if (m_isNavigating) {
            qWarning().noquote() << "[NavigationController] replace REJECTED: reentrant call during active transition";
            return false;
        }

        ReentrancyGuard guard(m_isNavigating);

        // 1. 先验证并执行 UI 呈现
        const bool ok = m_presenter->presentRoute(routeKey, NavigationDirection::None);
        if (!ok) {
            qWarning().noquote() << "[NavigationController] replace FAILED: presenter rejected route:" << routeKey;
            return false;
        }

        // 2. 呈现成功后才更新当前路由
        if (m_history) {
            m_history->replace(routeKey);
        }
        qDebug().noquote() << "[NavigationController] replace SUCCESS to:" << routeKey;
        return true;
    }

} // namespace ui::navigation
