#include "MainViewModel.h"

namespace ui::screen::main {
    MainViewModel::MainViewModel(QObject *parent)
        : BaseViewModel<MainViewModel, MainState>(parent) {
    }

    MainViewModel::~MainViewModel() = default;

    void MainViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::main
