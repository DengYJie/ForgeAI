#pragma once

#include "ui/base/BaseViewModel.h"
#include <QString>

namespace ui::navigation {
    class NavigationHistory;
}

namespace ui::screen::main {
    struct MainState {
        QString currentRoute = QStringLiteral("home");
        bool canGoBack = false;
        bool canGoForward = false;
        bool isPaneOpen = true;

        bool operator==(const MainState &other) const = default;
    };

    class MainViewModel : public BaseViewModel<MainViewModel, MainState> {
        Q_OBJECT

    public:
        explicit MainViewModel(QObject *parent = nullptr);

        ~MainViewModel() override;

        void navigateTo(const QString &route) const;

        void goBack() const;

        void goForward() const;

        void setPaneOpen(bool open);

        void togglePane();

        ui::navigation::NavigationHistory *history() const;

    Q_SIGNALS:
        void stateChanged(const ui::screen::main::MainState &state);

    protected:
        void emitStateChanged() override;

    private:
        ui::navigation::NavigationHistory *m_history = nullptr;
    };
} // namespace ui::screen::main
