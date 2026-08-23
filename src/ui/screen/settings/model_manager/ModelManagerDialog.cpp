#include "ModelManagerDialog.h"
#include "AddProviderDialog.h"
#include "AddModelDialog.h"
#include "core/model/ModelRegistry.h"
#include "application/usecase/settings/RefreshModelsUseCase.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/Design.h>

namespace ui::screen::settings::model_manager {

    ModelManagerDialog::ModelManagerDialog(
        core::model::ModelRegistry *registry,
        application::usecase::settings::RefreshModelsUseCase *refreshUseCase,
        QWidget *parent
    ) : QDialog(parent), m_registry(registry), m_refreshUseCase(refreshUseCase) {
        setWindowTitle(tr("模型与服务商"));
        setMinimumSize(860, 620);
        resize(880, 640);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false);

        setupUi();
        loadProviders();

        if (m_registry) {
            connect(m_registry, &core::model::ModelRegistry::providersChanged, this, &ModelManagerDialog::loadProviders);
        }

        if (m_refreshUseCase) {
            connect(m_refreshUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryStarted, this, [this](const QString &) {
                if (m_detailView) m_detailView->setRefreshing(true);
            });
            connect(m_refreshUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryFinished, this, [this](const QString &providerId, int count) {
                if (m_detailView) m_detailView->setRefreshing(false);
                fluent::status_info::Toast::showToast(
                    this,
                    tr("成功刷新 %1 个可用模型").arg(count),
                    fluent::status_info::Toast::Success
                );
                if (m_registry) {
                    auto p = m_registry->getProvider(providerId);
                    if (p.has_value() && m_detailView) {
                        m_detailView->setProvider(p.value());
                    }
                }
            });
            connect(m_refreshUseCase, &application::usecase::settings::RefreshModelsUseCase::discoveryFailed, this, [this](const QString &, const QString &err) {
                if (m_detailView) m_detailView->setRefreshing(false);
                fluent::status_info::Toast::showToast(
                    this,
                    tr("模型探测失败: %1").arg(err),
                    fluent::status_info::Toast::Error
                );
            });
        }
    }

    void ModelManagerDialog::setupUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. 顶部 Header
        auto *headerWidget = new QWidget(this);
        headerWidget->setFixedHeight(64);
        auto *headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(24, 12, 24, 12);
        headerLayout->setSpacing(12);

        auto *titleCol = new QVBoxLayout();
        titleCol->setContentsMargins(0, 0, 0, 0);
        titleCol->setSpacing(2);

        auto *titleLabel = new fluent::textfields::Label(tr("模型与服务商"), headerWidget);
        titleLabel->setFluentTypography(Typography::FontRole::Title);
        titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        auto *subtitleLabel = new fluent::textfields::Label(tr("管理大语言模型服务商凭据、API 端点与可用模型列表"), headerWidget);
        subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
        subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        titleCol->addWidget(titleLabel);
        titleCol->addWidget(subtitleLabel);
        headerLayout->addLayout(titleCol, 1);

        rootLayout->addWidget(headerWidget);

        // 2. 中间水平双栏容器
        auto *bodyWidget = new QWidget(this);
        auto *bodyLayout = new QHBoxLayout(bodyWidget);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);

        m_navPane = new ProviderNavigationPane(bodyWidget);
        m_detailView = new ProviderDetailView(bodyWidget);

        bodyLayout->addWidget(m_navPane);
        bodyLayout->addWidget(m_detailView, 1);
        rootLayout->addWidget(bodyWidget, 1);

        // 3. 底部 Action Bar
        auto *footerWidget = new QWidget(this);
        footerWidget->setFixedHeight(56);
        auto *footerLayout = new QHBoxLayout(footerWidget);
        footerLayout->setContentsMargins(24, 10, 24, 10);
        footerLayout->setSpacing(12);

        footerLayout->addStretch(1);
        auto *doneBtn = new fluent::basicinput::Button(footerWidget);
        doneBtn->setText(tr("完成"));
        doneBtn->setMinimumWidth(100);
        connect(doneBtn, &fluent::basicinput::Button::clicked, this, &QDialog::accept);
        footerLayout->addWidget(doneBtn);

        rootLayout->addWidget(footerWidget);

        // 4. 信号连接
        connect(m_navPane, &ProviderNavigationPane::providerSelected, this, [this](const QString &providerId) {
            if (!m_registry) return;
            auto p = m_registry->getProvider(providerId);
            if (p.has_value()) {
                m_detailView->setProvider(p.value());
            }
        });

        connect(m_navPane, &ProviderNavigationPane::addProviderRequested, this, &ModelManagerDialog::onAddProvider);

        connect(m_detailView, &ProviderDetailView::providerChanged, this, [this](const domain::model::ModelProvider &provider) {
            if (m_registry) {
                m_registry->saveProvider(provider);
            }
        });

        connect(m_detailView, &ProviderDetailView::providerDeleted, this, [this](const QString &providerId) {
            if (m_registry) {
                m_registry->deleteProvider(providerId);
            }
        });

        connect(m_detailView, &ProviderDetailView::refreshModelsRequested, this, [this](const QString &providerId) {
            if (m_refreshUseCase) {
                m_refreshUseCase->execute(providerId);
            }
        });

        connect(m_detailView, &ProviderDetailView::addModelRequested, this, &ModelManagerDialog::onAddModel);
    }

    void ModelManagerDialog::loadProviders() {
        if (!m_registry || !m_navPane) return;
        auto providers = m_registry->getActiveProviders();
        m_navPane->setProviders(providers);

        QString selectedId = m_navPane->currentSelectedId();
        if (!selectedId.isEmpty()) {
            auto p = m_registry->getProvider(selectedId);
            if (p.has_value() && m_detailView) {
                m_detailView->setProvider(p.value());
            }
        }
    }

    void ModelManagerDialog::onAddProvider() {
        AddProviderDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            auto provider = dlg.resultProvider();
            if (!provider.id.isEmpty() && m_registry) {
                m_registry->saveProvider(provider);
                m_navPane->selectProvider(provider.id);
                m_detailView->setProvider(provider);
            }
        }
    }

    void ModelManagerDialog::onAddModel(const QString &providerId) {
        AddModelDialog dlg(providerId, this);
        if (dlg.exec() == QDialog::Accepted) {
            auto model = dlg.resultModel();
            if (!model.id.isEmpty() && m_registry) {
                auto optProvider = m_registry->getProvider(providerId);
                if (optProvider.has_value()) {
                    auto provider = optProvider.value();
                    provider.models.append(model);
                    m_registry->saveProvider(provider);
                    m_detailView->setProvider(provider);
                }
            }
        }
    }

    void ModelManagerDialog::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
        const auto &colors = themeColorsRef();

        painter.fillRect(rect(), colors.bgSolid);
    }

    void ModelManagerDialog::onThemeUpdated() {
        update();
    }

} // namespace ui::screen::settings::model_manager
