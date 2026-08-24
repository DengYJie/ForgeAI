#include "ModelManagerViewModel.h"
#include "application/usecase/settings/GetModelsUseCase.h"
#include "application/usecase/settings/SaveProviderUseCase.h"
#include "application/usecase/settings/DeleteProviderUseCase.h"
#include "application/usecase/settings/RefreshModelsUseCase.h"
#include <QDebug>

namespace ui::screen::settings::model_manager {

    ModelManagerViewModel::ModelManagerViewModel(
        application::usecase::settings::GetModelsUseCase *getModelsUseCase,
        application::usecase::settings::SaveProviderUseCase *saveProviderUseCase,
        application::usecase::settings::DeleteProviderUseCase *deleteProviderUseCase,
        application::usecase::settings::RefreshModelsUseCase *refreshModelsUseCase,
        QObject *parent
    ) : BaseViewModel<ModelManagerViewModel, ModelManagerState>(parent),
        m_getModelsUseCase(getModelsUseCase),
        m_saveProviderUseCase(saveProviderUseCase),
        m_deleteProviderUseCase(deleteProviderUseCase),
        m_refreshModelsUseCase(refreshModelsUseCase) {

        if (m_getModelsUseCase) {
            connect(m_getModelsUseCase, &application::usecase::settings::GetModelsUseCase::modelsChanged,
                    this, &ModelManagerViewModel::loadProviders);
        }

        if (m_refreshModelsUseCase) {
            connect(m_refreshModelsUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryStarted,
                    this, [this](const QString &providerId) {
                updateState([providerId](ModelManagerState &s) {
                    s.isRefreshing = true;
                    s.refreshingProviderId = providerId;
                });
            });

            connect(m_refreshModelsUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryFinished,
                    this, [this](const QString &providerId, int count) {
                Q_EMIT toastRequested(tr("成功刷新 %1 个可用模型").arg(count), false);
                loadProviders();
                updateState([](ModelManagerState &s) {
                    s.isRefreshing = false;
                    s.refreshingProviderId.clear();
                });
            });

            connect(m_refreshModelsUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryFailed,
                    this, [this](const QString &, const QString &error) {
                Q_EMIT toastRequested(tr("模型探测失败: %1").arg(error), true);
                updateState([](ModelManagerState &s) {
                    s.isRefreshing = false;
                    s.refreshingProviderId.clear();
                });
            });
        }
    }

    void ModelManagerViewModel::loadProviders() {
        if (!m_getModelsUseCase) return;

        const auto providers = m_getModelsUseCase->getAllProviders();
        qInfo().noquote() << QStringLiteral("[ModelManagerViewModel] loadProviders: 从用例获取到 %1 个服务商").arg(providers.size());
        updateState([this, &providers](ModelManagerState &s) {
            s.providers = providers;
            applyProviderSelection(s, s.selectedProviderId);
            qInfo().noquote() << QStringLiteral("[ModelManagerViewModel] loadProviders 完成: 状态包含 %1 个服务商, 当前选中=%2")
                .arg(s.providers.size()).arg(s.selectedProviderId);
        });
    }

    void ModelManagerViewModel::selectProvider(const QString &providerId) {
        if (m_state.selectedProviderId == providerId) return;

        updateState([this, &providerId](ModelManagerState &s) {
            applyProviderSelection(s, providerId);
        });
    }

    void ModelManagerViewModel::saveProvider(const domain::model::ModelProvider &provider) {
        if (m_saveProviderUseCase) {
            m_saveProviderUseCase->execute(provider);
        }

        updateState([this, &provider](ModelManagerState &s) {
            for (auto &p : s.providers) {
                if (p.id == provider.id) {
                    p = provider;
                    break;
                }
            }
            if (s.selectedProviderId == provider.id) {
                s.selectedProvider = provider;
            }
        });
    }

    void ModelManagerViewModel::deleteProvider(const QString &providerId) {
        if (m_deleteProviderUseCase) {
            m_deleteProviderUseCase->execute(providerId);
        }

        updateState([this, &providerId](ModelManagerState &s) {
            int deleteIndex = -1;
            for (int i = 0; i < s.providers.size(); ++i) {
                if (s.providers[i].id == providerId) {
                    deleteIndex = i;
                    break;
                }
            }

            if (deleteIndex >= 0) {
                s.providers.removeAt(deleteIndex);
            }

            QString nextSelectedId;
            if (!s.providers.isEmpty()) {
                const int nextIndex = qBound(0, deleteIndex, s.providers.size() - 1);
                nextSelectedId = s.providers[nextIndex].id;
            }
            applyProviderSelection(s, nextSelectedId);
        });
    }

    void ModelManagerViewModel::refreshModels(const QString &providerId) {
        if (m_refreshModelsUseCase) {
            m_refreshModelsUseCase->execute(providerId);
        }
    }

    void ModelManagerViewModel::addProvider(const domain::model::ModelProvider &provider) {
        if (m_saveProviderUseCase) {
            m_saveProviderUseCase->execute(provider);
        }

        updateState([this, &provider](ModelManagerState &s) {
            bool found = false;
            for (auto &p : s.providers) {
                if (p.id == provider.id) {
                    p = provider;
                    found = true;
                    break;
                }
            }
            if (!found) {
                s.providers.append(provider);
            }
            applyProviderSelection(s, provider.id);
        });
    }

    void ModelManagerViewModel::addModel(const domain::model::ProviderModel &binding) {
        Q_UNUSED(binding)
        // 预留自定义模型添加
    }

    void ModelManagerViewModel::applyProviderSelection(ModelManagerState &state, const QString &preferredId) {
        state.selectedProviderId.clear();
        state.selectedProvider.reset();
        state.selectedProviderModels.clear();

        if (state.providers.isEmpty()) {
            return;
        }

        QString targetId = preferredId;
        if (targetId.isEmpty()) {
            targetId = state.providers.first().id;
        }

        for (const auto &p : state.providers) {
            if (p.id == targetId) {
                state.selectedProviderId = p.id;
                state.selectedProvider = p;
                if (m_getModelsUseCase) {
                    state.selectedProviderModels = m_getModelsUseCase->getModelsForProvider(p.id);
                }
                return;
            }
        }

        state.selectedProviderId = state.providers.first().id;
        state.selectedProvider = state.providers.first();
        if (m_getModelsUseCase) {
            state.selectedProviderModels = m_getModelsUseCase->getModelsForProvider(state.selectedProviderId);
        }
    }

} // namespace ui::screen::settings::model_manager
