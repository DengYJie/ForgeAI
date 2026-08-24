#pragma once

#include <QWidget>

#include "ModelManagerViewModel.h"

namespace ui::screen::settings::model_manager {
    class ProviderNavigationPane;
    class ProviderDetailView;

    class ModelManagerPage : public QWidget {
        Q_OBJECT
    public:
        explicit ModelManagerPage(ModelManagerViewModel *viewModel, QWidget *parent = nullptr);

    private:
        void renderState(const ModelManagerState &state);
        void onAddProvider();
        void onAddModel(const QString &providerId);

        ModelManagerViewModel *m_viewModel = nullptr;
        ProviderNavigationPane *m_navPane = nullptr;
        ProviderDetailView *m_detailView = nullptr;
    };
} // namespace ui::screen::settings::model_manager
