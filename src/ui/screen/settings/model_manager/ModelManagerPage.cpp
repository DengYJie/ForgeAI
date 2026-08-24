#include "ModelManagerPage.h"

#include "AddModelDialog.h"
#include "AddProviderDialog.h"
#include "ProviderDetailView.h"
#include "ProviderNavigationPane.h"

#include <QHBoxLayout>
#include <FluentQt/StatusInfo.h>

namespace ui::screen::settings::model_manager {
    ModelManagerPage::ModelManagerPage(ModelManagerViewModel *viewModel, QWidget *parent)
        : QWidget(parent), m_viewModel(viewModel) {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        m_navPane = new ProviderNavigationPane(this);
        m_navPane->setFixedWidth(240);
        m_detailView = new ProviderDetailView(this);
        layout->addWidget(m_navPane);
        layout->addWidget(m_detailView, 1);

        if (m_viewModel) {
            m_viewModel->observe(this, &ModelManagerPage::renderState);
            connect(m_viewModel, &ModelManagerViewModel::toastRequested, this, [this](const QString &text, bool error) {
                fluent::status_info::Toast::showToast(this, text,
                    error ? fluent::status_info::Toast::Error : fluent::status_info::Toast::Success);
            });
            m_viewModel->loadProviders();
        }
        connect(m_navPane, &ProviderNavigationPane::providerSelected, this, [this](const QString &id) {
            if (m_viewModel) m_viewModel->selectProvider(id);
        });
        connect(m_navPane, &ProviderNavigationPane::addProviderRequested, this, &ModelManagerPage::onAddProvider);
        connect(m_detailView, &ProviderDetailView::addModelRequested, this, &ModelManagerPage::onAddModel);
        connect(m_detailView, &ProviderDetailView::providerChanged, this, [this](const domain::model::ModelProvider &provider) {
            if (m_viewModel) m_viewModel->saveProvider(provider);
        });
        connect(m_detailView, &ProviderDetailView::refreshModelsRequested, this, [this](const QString &id) {
            if (m_viewModel) m_viewModel->refreshModels(id);
        });
    }

    void ModelManagerPage::renderState(const ModelManagerState &state) {
        m_navPane->setProviders(state.providers);
        if (!state.selectedProviderId.isEmpty()) m_navPane->selectProvider(state.selectedProviderId);
        m_detailView->setProviderData(state.selectedProvider, state.selectedProviderModels);
        m_detailView->setRefreshing(state.isRefreshing);
    }

    void ModelManagerPage::onAddProvider() {
        AddProviderDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted && m_viewModel) m_viewModel->addProvider(dialog.resultProvider());
    }

    void ModelManagerPage::onAddModel(const QString &providerId) {
        AddModelDialog dialog(providerId, this);
        if (dialog.exec() == QDialog::Accepted && m_viewModel) m_viewModel->addModel(providerId, dialog.resultModel());
    }
} // namespace ui::screen::settings::model_manager
