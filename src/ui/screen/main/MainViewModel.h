#pragma once

#include "ui/base/BaseViewModel.h"
#include <QString>

namespace ui::screen::main {
    struct MainState {
        bool operator==(const MainState &) const { return true; }
    };

    class MainViewModel : public BaseViewModel<MainViewModel, MainState> {
        Q_OBJECT

    public:
        explicit MainViewModel(QObject *parent = nullptr);

        ~MainViewModel() override;

    Q_SIGNALS:
        void stateChanged(const ui::screen::main::MainState &state);

    protected:
        void emitStateChanged() override;

    };
} // namespace ui::screen::main
