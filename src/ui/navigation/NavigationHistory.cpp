#include "ui/navigation/NavigationHistory.h"

namespace ui::navigation {
    NavigationHistory::NavigationHistory(QObject *parent)
        : QObject(parent) {
    }

    bool NavigationHistory::canGoBack() const {
        return !m_backStack.isEmpty();
    }

    bool NavigationHistory::canGoForward() const {
        return !m_forwardStack.isEmpty();
    }

    QString NavigationHistory::currentRoute() const {
        return m_currentRoute;
    }

    const QStringList &NavigationHistory::backStack() const {
        return m_backStack;
    }

    const QStringList &NavigationHistory::forwardStack() const {
        return m_forwardStack;
    }

    void NavigationHistory::push(const QString &newRoute) {
        if (newRoute.isEmpty() || m_currentRoute == newRoute) return;

        if (!m_currentRoute.isEmpty()) {
            m_backStack.append(m_currentRoute);
            Q_EMIT canGoBackChanged(canGoBack());
        }

        m_currentRoute = newRoute;
        Q_EMIT currentRouteChanged(m_currentRoute);

        if (!m_forwardStack.isEmpty()) {
            m_forwardStack.clear();
            Q_EMIT canGoForwardChanged(canGoForward());
        }
    }

    QString NavigationHistory::goBack() {
        if (m_backStack.isEmpty()) return QString();

        if (!m_currentRoute.isEmpty()) {
            m_forwardStack.append(m_currentRoute);
            Q_EMIT canGoForwardChanged(canGoForward());
        }

        m_currentRoute = m_backStack.takeLast();
        Q_EMIT canGoBackChanged(canGoBack());
        Q_EMIT currentRouteChanged(m_currentRoute);

        Q_EMIT navigatedBack(m_currentRoute);
        return m_currentRoute;
    }

    QString NavigationHistory::goForward() {
        if (m_forwardStack.isEmpty()) return QString();

        if (!m_currentRoute.isEmpty()) {
            m_backStack.append(m_currentRoute);
            Q_EMIT canGoBackChanged(canGoBack());
        }

        m_currentRoute = m_forwardStack.takeLast();
        Q_EMIT canGoForwardChanged(canGoForward());
        Q_EMIT currentRouteChanged(m_currentRoute);

        Q_EMIT navigatedForward(m_currentRoute);
        return m_currentRoute;
    }

    void NavigationHistory::clear() {
        m_currentRoute.clear();
        Q_EMIT currentRouteChanged(m_currentRoute);

        if (!m_backStack.isEmpty()) {
            m_backStack.clear();
            Q_EMIT canGoBackChanged(canGoBack());
        }
        if (!m_forwardStack.isEmpty()) {
            m_forwardStack.clear();
            Q_EMIT canGoForwardChanged(canGoForward());
        }
    }
} // namespace ui::navigation
