#include "SettingsPage.h"

#include <algorithm>
#include <functional>
#include <utility>

#include <QGridLayout>
#include <QMap>
#include <QPalette>
#include <QDebug>
#include <QShowEvent>
#include <QVBoxLayout>

#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/TextFields.h>

#include "SettingsCoordinator.h"
#include "SettingsUIRegistry.h"
#include "SettingsViewModel.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/window/NavigationWindow.h"

using namespace fluent;

namespace ui::screen::settings {
    namespace {
        constexpr int kStackedCardWidthThreshold = 500;
        constexpr int kNarrowWidthThreshold = 640;

        class SettingsScrollView : public fluent::scrolling::ScrollView {
        public:
            explicit SettingsScrollView(QWidget *parent = nullptr)
                : fluent::scrolling::ScrollView(parent) {
                setAttribute(Qt::WA_OpaquePaintEvent, false);
                viewport()->setAutoFillBackground(false);
                QPalette pal = palette();
                pal.setColor(QPalette::Window, Qt::transparent);
                pal.setColor(QPalette::Base, Qt::transparent);
                setPalette(pal);
            }

        protected:
            void paintEvent(QPaintEvent *) override {
            }

            void onThemeUpdated() override {
                fluent::scrolling::ScrollView::onThemeUpdated();
                viewport()->setAutoFillBackground(false);

                QPalette pal = viewport()->palette();
                pal.setColor(QPalette::Window, Qt::transparent);
                pal.setColor(QPalette::Base, Qt::transparent);
                viewport()->setPalette(pal);
            }
        };

        class SecondaryLabel : public fluent::textfields::Label {
        public:
            SecondaryLabel(const QString &text, QWidget *parent = nullptr)
                : Label(text, parent) {
                setTextColorRole(TextColorRole::Secondary);
            }
        };

        class LazyProviderPage final : public QWidget {
        public:
            explicit LazyProviderPage(std::function<QWidget *(QWidget *)> factory, QWidget *parent = nullptr)
                : QWidget(parent)
                , m_factory(std::move(factory)) {
                setAutoFillBackground(false);
                auto *layout = new QVBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(0);
            }

        protected:
            void showEvent(QShowEvent *event) override {
                QWidget::showEvent(event);
                ensureCreated();
            }

        private:
            void ensureCreated() {
                if (m_created || !m_factory) return;
                auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
                if (!layout) return;
                QWidget *page = m_factory(this);
                if (!page) return;
                layout->addWidget(page, 1);
                m_created = true;
                qDebug().noquote() << "[SettingsPage] lazy provider page created";
            }

            std::function<QWidget *(QWidget *)> m_factory;
            bool m_created = false;
        };
    } // namespace

    class SettingsCardItem : public fluent::layout::Card {
    public:
        SettingsCardItem(const QString &iconGlyph,
                         const QString &title,
                         const QString &subtitle,
                         QWidget *trailingWidget,
                         QWidget *parent = nullptr)
            : fluent::layout::Card(parent)
              , m_layout(new QGridLayout(this))
              , m_trailing(trailingWidget) {
            setObjectName(QStringLiteral("settingsCardItem"));
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            setMinimumHeight(68);

            m_layout->setContentsMargins(16, 12, 16, 12);
            m_layout->setHorizontalSpacing(16);
            m_layout->setVerticalSpacing(8);

            if (!iconGlyph.isEmpty()) {
                auto *iconView = new fluent::FontIcon(iconGlyph, this);
                iconView->setObjectName(QStringLiteral("settingsCardIcon"));
                iconView->setIconSize(Typography::IconSize::Standard);
                iconView->setFixedSize(28, 28);
                m_layout->addWidget(iconView, 0, 0, Qt::AlignVCenter);
            }

            auto *textColumn = new QWidget(this);
            auto *textLayout = new QVBoxLayout(textColumn);
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(2);

            auto *titleLabel = new fluent::textfields::Label(title, textColumn);
            titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
            titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
            textLayout->addWidget(titleLabel);

            if (!subtitle.isEmpty()) {
                auto *subtitleLabel = new SecondaryLabel(subtitle, textColumn);
                subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
                subtitleLabel->setWordWrap(true);
                textLayout->addWidget(subtitleLabel);
            }

            m_layout->addWidget(textColumn, 0, 1, Qt::AlignVCenter);

            if (m_trailing) {
                m_trailing->setParent(this);
                m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
            }

            updateLayoutMode();
        }

        void setStacked(bool stacked) {
            if (m_stacked == stacked || !m_trailing) return;
            m_stacked = stacked;

            m_layout->removeWidget(m_trailing);
            if (m_stacked) {
                m_layout->addWidget(m_trailing, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
                setMinimumHeight(100);
            } else {
                m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
                setMinimumHeight(68);
            }
            m_trailing->show();
            updateGeometry();
        }

    protected:
        void resizeEvent(QResizeEvent *event) override {
            Card::resizeEvent(event);
            updateLayoutMode();
        }

    private:
        void updateLayoutMode() {
            setStacked(width() > 0 && width() < kStackedCardWidthThreshold);
        }

        QGridLayout *m_layout = nullptr;
        QWidget *m_trailing = nullptr;
        bool m_stacked = false;
    };

    SettingsPage::SettingsPage(SettingsViewModel *viewModel,
                               SettingsUIRegistry *uiRegistry,
                               SettingsCoordinator *coordinator,
                               fluent::navigation::NavigationView *navigationView,
                               ui::navigation::NavigationPanel *globalNavigationPanel,
                               QObject *parent)
        : QObject(parent)
        , m_viewModel(viewModel)
        , m_uiRegistry(uiRegistry)
        , m_coordinator(coordinator)
        , m_navigationView(navigationView)
        , m_globalNavigationPanel(globalNavigationPanel) {
        setObjectName(QStringLiteral("settingsPage"));

        setupUi();

        if (m_viewModel) {
            m_viewModel->observe(this, &SettingsPage::render);
        }
        if (m_coordinator) {
            connect(m_coordinator, &SettingsCoordinator::providerPageRequested,
                    this, &SettingsPage::selectProviderPage);
        }
    }

    void SettingsPage::render(const SettingsState &state) {
        Q_UNUSED(state);
    }

    void SettingsPage::setupUi() {
        auto *panelParent = static_cast<QWidget *>(m_navigationView);
        if (!panelParent && m_globalNavigationPanel) {
            panelParent = m_globalNavigationPanel->parentWidget();
        }

        m_settingsNavigationPanel = new ui::navigation::NavigationPanel(panelParent);
        m_settingsNavigationPanel->setObjectName(QStringLiteral("settingsNavigationPanel"));
        m_settingsNavigationPanel->setPaneToggleButtonVisible(false);
        m_settingsNavigationPanel->setBackButtonVisible(false);
        m_settingsNavigationPanel->hide();
    }

    void SettingsPage::registerProviderRoutes(NavigationWindow *window) {
        if (!window || !m_uiRegistry || !m_settingsNavigationPanel) return;
        if (m_navigationWindow == window && !m_routeToDescriptor.isEmpty()) return;
        m_navigationWindow = window;

        const auto descriptors = buildProviderPageDescriptors();
        QString previousCategory;
        for (const auto &descriptor : descriptors) {
            if (descriptor.providerId.isEmpty()) continue;

            if (descriptor.category != previousCategory) {
                m_settingsNavigationPanel->addSectionHeader(descriptor.category);
                previousCategory = descriptor.category;
            }

            const QString routeKey = routeKeyForProvider(descriptor.providerId);
            const QString icon = descriptor.iconGlyph.isEmpty() ? Typography::Icons::Settings : descriptor.iconGlyph;
            m_settingsNavigationPanel->addItem(routeKey, icon, descriptor.title);

            m_routeToDescriptor.insert(routeKey, descriptor);
            m_providerToRoute.insert(descriptor.providerId, routeKey);
            if (m_initialRouteKey.isEmpty()) {
                m_initialRouteKey = routeKey;
            }

            QWidget *page = new LazyProviderPage(
                [this, descriptor](QWidget *parent) {
                    return descriptor.hasCustomPage()
                               ? createCustomProviderPage(descriptor, parent)
                               : createGenericProviderPage(descriptor, parent);
                },
                window
            );
            window->addContentPage(routeKey, page);
        }

        connect(m_settingsNavigationPanel, &ui::navigation::NavigationPanel::itemSelected,
                window, &NavigationWindow::switchTo);
        connect(window, &NavigationWindow::routeChanged,
                this, &SettingsPage::handleRouteChanged);
    }

    QList<SettingsProviderPageDescriptor> SettingsPage::buildProviderPageDescriptors() const {
        QMap<QString, SettingsProviderPageDescriptor> descriptors;

        if (m_viewModel) {
            const auto providers = m_viewModel->state().providers;
            for (const auto &provider : providers) {
                SettingsProviderPageDescriptor descriptor;
                descriptor.providerId = provider.id;
                descriptor.category = provider.category;
                descriptor.title = provider.title;
                descriptors.insert(provider.id, descriptor);
            }
        }

        if (m_uiRegistry) {
            for (const auto &factory : m_uiRegistry->sortedFactories()) {
                if (!factory) continue;
                auto &descriptor = descriptors[factory->providerId()];
                descriptor.providerId = factory->providerId();
                if (descriptor.category.isEmpty()) descriptor.category = factory->categoryDisplayName();
                if (descriptor.title.isEmpty()) descriptor.title = factory->categoryDisplayName();
                if (descriptor.iconGlyph.isEmpty()) descriptor.iconGlyph = factory->iconGlyph();
                descriptor.order = qMin(descriptor.order, factory->categoryOrder());
                descriptor.factories.append(factory);
            }

            for (const auto &factory : m_uiRegistry->providerPageFactories()) {
                if (!factory) continue;
                auto &descriptor = descriptors[factory->providerId()];
                descriptor.providerId = factory->providerId();
                if (descriptor.iconGlyph.isEmpty()) descriptor.iconGlyph = factory->iconGlyph();
                descriptor.order = qMin(descriptor.order, factory->categoryOrder());
                descriptor.pageFactory = factory.get();
            }
        }

        auto result = descriptors.values();
        std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
            if (a.order != b.order) return a.order < b.order;
            if (a.category != b.category) return a.category < b.category;
            return a.title < b.title;
        });
        return result;
    }

    QWidget *SettingsPage::createGenericProviderPage(const SettingsProviderPageDescriptor &descriptor, QWidget *parent) {
        auto *scrollArea = new SettingsScrollView(parent);
        scrollArea->setObjectName(QStringLiteral("settingsProviderScrollArea"));
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
        scrollArea->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

        auto *viewport = new QWidget(scrollArea);
        viewport->setObjectName(QStringLiteral("settingsProviderViewport"));
        viewport->setAutoFillBackground(false);

        auto *contentLayout = new QVBoxLayout(viewport);
        contentLayout->setContentsMargins(36, 28, 36, 36);
        contentLayout->setSpacing(6);
        m_pageLayouts.append(contentLayout);

        auto *titleLabel = new fluent::textfields::Label(descriptor.title, viewport);
        titleLabel->setObjectName(QStringLiteral("settingsProviderTitle"));
        titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        titleLabel->setFluentTypography(Typography::FontRole::Title);
        contentLayout->addWidget(titleLabel);
        contentLayout->addSpacing(12);

        for (const auto &factory : descriptor.factories) {
            if (!factory) continue;
            QWidget *control = factory->createControlWidget(viewport);
            contentLayout->addWidget(createSettingsCard(
                factory->iconGlyph(),
                factory->title(),
                factory->subtitle(),
                control,
                viewport
            ));
        }
        contentLayout->addStretch(1);

        scrollArea->setWidget(viewport);
        return scrollArea;
    }

    QWidget *SettingsPage::createCustomProviderPage(const SettingsProviderPageDescriptor &descriptor, QWidget *parent) {
        auto *page = new QWidget(parent);
        page->setObjectName(QStringLiteral("settingsCustomProviderPage"));
        page->setAutoFillBackground(false);

        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(36, 28, 36, 36);
        layout->setSpacing(0);
        m_pageLayouts.append(layout);

        auto *titleLabel = new fluent::textfields::Label(descriptor.title, page);
        titleLabel->setObjectName(QStringLiteral("settingsProviderTitle"));
        titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        titleLabel->setFluentTypography(Typography::FontRole::Title);
        layout->addWidget(titleLabel);
        layout->addSpacing(16);

        QWidget *customPage = descriptor.pageFactory->createProviderPage(page);
        layout->addWidget(customPage, 1);
        return page;
    }

    QWidget *SettingsPage::createSectionHeader(const QString &title, QWidget *parent) {
        auto *label = new fluent::textfields::Label(title, parent);
        label->setObjectName(QStringLiteral("settingsSectionHeader"));
        label->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        label->setFluentTypography(Typography::FontRole::BodyStrong);
        label->setContentsMargins(2, 12, 0, 4);
        return label;
    }

    QWidget *SettingsPage::createSettingsCard(const QString &iconGlyph,
                                              const QString &title,
                                              const QString &subtitle,
                                              QWidget *trailingWidget,
                                              QWidget *parent) {
        auto *card = new SettingsCardItem(iconGlyph, title, subtitle, trailingWidget, parent);
        m_cards.append(card);
        return card;
    }

    QString SettingsPage::routeKeyForProvider(const QString &providerId) const {
        return QStringLiteral("settings.provider.%1").arg(providerId);
    }

    void SettingsPage::selectProviderPage(const QString &providerId) {
        const QString routeKey = m_providerToRoute.value(providerId);
        if (m_navigationWindow && !routeKey.isEmpty()) {
            m_navigationWindow->switchTo(routeKey);
        }
    }

    void SettingsPage::handleRouteChanged(const QString &routeKey) {
        const bool providerRoute = routeKey.startsWith(QStringLiteral("settings.provider."));
        qDebug().noquote() << "[SettingsPage] routeChanged route=" << routeKey
                           << "providerRoute=" << providerRoute;

        if (providerRoute) {
            attachSettingsNavigation();
            if (m_settingsNavigationPanel
                && m_settingsNavigationPanel->currentRouteKey() != routeKey) {
                m_settingsNavigationPanel->setCurrentItem(routeKey);
            }
        } else {
            detachSettingsNavigation();
        }
    }

    void SettingsPage::attachSettingsNavigation() {
        if (!m_navigationView || !m_settingsNavigationPanel || m_settingsNavigationAttached) return;

        if (m_navigationView->mainChromeWidget()) {
            m_detachedMainChromeWidget = m_navigationView->takeMainChromeWidget();
            if (m_detachedMainChromeWidget) {
                m_detachedMainChromeWidget->hide();
            }
        }

        m_navigationView->setMainChromeWidget(
            m_settingsNavigationPanel,
            fluent::WidgetOwnership::Reparented
        );
        syncSettingsNavigationWithNavView();
        bindSettingsNavigationToNavView();
        m_settingsNavigationPanel->show();
        m_settingsNavigationAttached = true;
        qDebug().noquote() << "[SettingsPage] attach settings navigation";
    }

    void SettingsPage::detachSettingsNavigation() {
        if (!m_navigationView || !m_settingsNavigationAttached) return;

        unbindSettingsNavigationFromNavView();

        if (m_navigationView->mainChromeWidget() == m_settingsNavigationPanel) {
            m_navigationView->takeMainChromeWidget();
            m_settingsNavigationPanel->setParent(m_navigationView);
            m_settingsNavigationPanel->hide();
        }

        if (m_detachedMainChromeWidget) {
            m_navigationView->setMainChromeWidget(
                m_detachedMainChromeWidget,
                fluent::WidgetOwnership::Reparented
            );
            m_detachedMainChromeWidget->show();
            m_detachedMainChromeWidget.clear();
        } else if (m_globalNavigationPanel) {
            m_globalNavigationPanel->show();
        }

        m_settingsNavigationAttached = false;
        qDebug().noquote() << "[SettingsPage] detach settings navigation";
    }

    void SettingsPage::bindSettingsNavigationToNavView() {
        if (!m_navigationView || !m_settingsNavigationPanel || !m_navigationViewConnections.isEmpty()) return;

        m_navigationViewConnections.append(connect(
            m_navigationView,
            &fluent::navigation::NavigationView::effectiveDisplayModeChanged,
            this,
            [this](fluent::navigation::NavigationView::DisplayMode) {
                syncSettingsNavigationWithNavView();
            }
        ));

        m_navigationViewConnections.append(connect(
            m_navigationView,
            &fluent::navigation::NavigationView::paneOpenChanged,
            this,
            [this](bool) {
                syncSettingsNavigationWithNavView();
            }
        ));
    }

    void SettingsPage::unbindSettingsNavigationFromNavView() {
        for (const auto &connection : std::as_const(m_navigationViewConnections)) {
            QObject::disconnect(connection);
        }
        m_navigationViewConnections.clear();
    }

    void SettingsPage::syncSettingsNavigationWithNavView() {
        if (!m_navigationView || !m_settingsNavigationPanel) return;

        using DisplayMode = fluent::navigation::NavigationView::DisplayMode;
        const auto mode = m_navigationView->effectiveDisplayMode();
        const bool top = (mode == DisplayMode::Top);
        m_settingsNavigationPanel->setOrientation(top ? Qt::Horizontal : Qt::Vertical);
        m_settingsNavigationPanel->setCompacted(!m_navigationView->isPaneOpen());
    }

    void SettingsPage::updateResponsiveLayout(int availableWidth) {
        const int fallbackWidth = m_navigationView ? m_navigationView->width() : 0;
        const int w = availableWidth > 0 ? availableWidth : fallbackWidth;
        const bool narrow = w > 0 && w < kNarrowWidthThreshold;
        const int marginH = narrow ? 20 : 36;

        for (auto *layout : m_pageLayouts) {
            if (layout) layout->setContentsMargins(marginH, 24, marginH, 36);
        }

        for (auto *card : m_cards) {
            if (card) {
                card->setStacked(narrow || (card->width() > 0 && card->width() < kStackedCardWidthThreshold));
            }
        }
    }
} // namespace ui::screen::settings
