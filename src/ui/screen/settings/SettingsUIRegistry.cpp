#include "SettingsUIRegistry.h"

namespace ui::screen::settings {
    SettingsUIRegistry &SettingsUIRegistry::instance() {
        static SettingsUIRegistry registry;
        return registry;
    }

    void SettingsUIRegistry::registerFactory(std::shared_ptr<ISettingsUIFactory> factory) {
        if (factory) {
            m_factories.append(factory);
        }
    }

    QList<std::shared_ptr<ISettingsUIFactory> > SettingsUIRegistry::allFactories() const {
        return m_factories;
    }
} // namespace ui::screen::settings
