#include "ui/screen/settings/SettingsNavigationModule.h"

#include <FluentQt/Design.h>
#include <QDebug>
#include <QVBoxLayout>

#include "ui/animation/AnimatedSettingsVisualSource.h"
#include "ui/navigation/NavigationPanel.h"
#include "ui/screen/settings/SettingsDescriptorBuilder.h"
#include "ui/screen/settings/SettingsPageFactory.h"

namespace ui::screen::settings {

    namespace {
        constexpr int kNarrowThreshold = 800;
    }

    SettingsNavigationModule::SettingsNavigationModule(
        SettingsUIRegistry *uiRegistry,
        QObject *parent
    )
        : QObject(parent)
        , m_uiRegistry(uiRegistry)
        , m_panel(std::make_unique<ui::navigation::NavigationPanel>(nullptr)) {
        setObjectName(QStringLiteral("settingsNavigationModule"));

        m_panel->setObjectName(QStringLiteral("settingsNavigationPanel"));
        m_panel->setPaneToggleButtonVisible(false);
        m_panel->setBackButtonVisible(false);
        m_panel->hide();
    }

    SettingsNavigationModule::~SettingsNavigationModule() = default;

    bool SettingsNavigationModule::install(ui::navigation::INavigationRegistrar &registrar) {
        if (!m_uiRegistry || !m_panel) {
            qWarning().noquote() << "[SettingsNavigationModule] install FAILED: null registry or panel already released";
            return false;
        }

        if (m_isInstalled) {
            qWarning().noquote() << "[SettingsNavigationModule] install REJECTED: already installed";
            Q_ASSERT_X(false, "SettingsNavigationModule::install", "Module supports single installation only");
            return false;
        }

        // 1. 提取所有 Provider 描述符
        const auto descriptors = SettingsDescriptorBuilder::buildDescriptors(m_uiRegistry);
        if (descriptors.isEmpty()) {
            qDebug().noquote() << "[SettingsNavigationModule] install skipped: no provider descriptors registered";
            return false;
        }

        // 2. 注册 settings 专属导航面板到宿主 Surface 管理器（显式移交所有权）
        auto *rawPanel = m_panel.release();
        if (!registrar.registerSurface(QStringLiteral("settings"), rawPanel)) {
            qWarning().noquote() << "[SettingsNavigationModule] install FAILED: registrar rejected surface 'settings'";
            delete rawPanel;
            return false;
        }
        m_installedPanel = rawPanel;
        m_isInstalled = true;

        // 3. 严格遵循：构建描述符 -> 注册 route -> 成功后 addItem -> 记录 initialRoute
        QString previousCategory;
        for (const auto &descriptor : descriptors) {
            if (descriptor.providerId.isEmpty()) continue;

            const QString routeKey = routeKeyForProvider(descriptor.providerId);
            QWidget *page = SettingsPageFactory::createLazyPage(
                descriptor,
                m_pageLayouts,
                m_cards
            );

            // 严格先向 registrar 注册路由
            const bool registered = registrar.registerRoute(routeKey, page, QStringLiteral("settings"));
            if (!registered) {
                qWarning().noquote() << "[SettingsNavigationModule] registerRoute FAILED for:" << routeKey << "skipping item";
                delete page;
                continue;
            }

            // 仅在路由注册成功后才向导航面板与内部索引写入该项
            if (descriptor.category != previousCategory) {
                m_installedPanel->addSectionHeader(descriptor.category);
                previousCategory = descriptor.category;
            }

            const QString icon = descriptor.iconGlyph.isEmpty() ? Typography::Icons::Settings : descriptor.iconGlyph;
            m_installedPanel->addItem(routeKey, icon, descriptor.title);

            m_routeToDescriptor.insert(routeKey, descriptor);
            m_providerToRoute.insert(descriptor.providerId, routeKey);
            if (m_initialRouteKey.isEmpty()) {
                m_initialRouteKey = routeKey;
            }
        }

        qDebug().noquote() << "[SettingsNavigationModule] install SUCCESS, initialRoute:" << m_initialRouteKey;
        return !m_initialRouteKey.isEmpty();
    }

    std::optional<ui::navigation::NavigationItemDescriptor> SettingsNavigationModule::entryDescriptor() const {
        if (m_initialRouteKey.isEmpty()) {
            return std::nullopt;
        }

        ui::navigation::NavigationItemDescriptor item;
        item.routeKey = m_initialRouteKey;
        item.iconGlyph = Typography::Icons::Settings;
        item.text = tr("设置");
        item.position = ui::navigation::NavigationItemPosition::Bottom;
        item.selectable = true;
        item.visualSource = std::make_shared<ui::animation::AnimatedSettingsVisualSource>();

        return item;
    }

    void SettingsNavigationModule::updateResponsiveLayout(int availableWidth) {
        if (availableWidth <= 0) return;

        const bool narrow = availableWidth < kNarrowThreshold;
        const int marginH = narrow ? 20 : 36;
        const int marginV = narrow ? 16 : 24;

        for (auto *layout : m_pageLayouts) {
            if (layout) {
                layout->setContentsMargins(marginH, marginV, marginH, marginV);
            }
        }
    }

    QString SettingsNavigationModule::routeKeyForProvider(const QString &providerId) const {
        return QStringLiteral("settings.provider.") + providerId;
    }

} // namespace ui::screen::settings
