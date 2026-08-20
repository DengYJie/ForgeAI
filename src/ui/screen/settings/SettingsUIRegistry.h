#pragma once
#include <memory>
#include <QList>
#include "ISettingsUIFactory.h"

namespace ui::screen::settings {
    class SettingsUIRegistry {
    public:
        static SettingsUIRegistry &instance();

        void registerFactory(std::shared_ptr<ISettingsUIFactory> factory);

        QList<std::shared_ptr<ISettingsUIFactory> > allFactories() const;

    private:
        SettingsUIRegistry() = default;

        QList<std::shared_ptr<ISettingsUIFactory> > m_factories;
    };
} // namespace ui::screen::settings
