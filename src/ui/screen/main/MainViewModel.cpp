#include "MainViewModel.h"

#include "ui/navigation/NavigationHistory.h"

namespace ui::screen::main {
    MainViewModel::MainViewModel(QObject *parent)
        : BaseViewModel<MainViewModel, MainState>(parent),
          m_history(new ui::navigation::NavigationHistory(this)) {
        connect(m_history, &ui::navigation::NavigationHistory::canGoBackChanged, this, [this](bool canGoBack) {
            updateState([canGoBack](MainState &s) { s.canGoBack = canGoBack; });
        });

        connect(m_history, &ui::navigation::NavigationHistory::canGoForwardChanged, this, [this](bool canGoForward) {
            updateState([canGoForward](MainState &s) { s.canGoForward = canGoForward; });
        });

        connect(m_history, &ui::navigation::NavigationHistory::currentRouteChanged, this, [this](const QString &route) {
            updateState([route](MainState &s) { s.currentRoute = route; });
        });

        m_history->push(m_state.currentRoute);
    }

    MainViewModel::~MainViewModel() = default;

    void MainViewModel::navigateTo(const QString &route) const {
        if (m_history) {
            m_history->push(route);
        }
    }

    void MainViewModel::goBack() const {
        if (m_history) {
            m_history->goBack();
        }
    }

    void MainViewModel::goForward() const {
        if (m_history) {
            m_history->goForward();
        }
    }

    void MainViewModel::setPaneOpen(bool open) {
        updateState([open](MainState &s) { s.isPaneOpen = open; });
    }

    void MainViewModel::togglePane() {
        updateState([](MainState &s) { s.isPaneOpen = !s.isPaneOpen; });
    }

    ui::navigation::NavigationHistory *MainViewModel::history() const {
        return m_history;
    }

    void MainViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::main
