#include "SettingsUIRegistry.h"
#include <algorithm>

namespace ui::screen::settings {
    void SettingsUIRegistry::registerFactory(std::shared_ptr<ISettingsUIFactory> factory) {
        if (factory) {
            m_factories.append(factory);
        }
    }

    QList<std::shared_ptr<ISettingsUIFactory>> SettingsUIRegistry::allFactories() const {
        return m_factories;
    }

    QList<std::shared_ptr<ISettingsUIFactory>> SettingsUIRegistry::sortedFactories() const {
        auto sorted = m_factories;
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
            if (a->categoryOrder() != b->categoryOrder()) {
                return a->categoryOrder() < b->categoryOrder();
            }
            if (a->categoryId() != b->categoryId()) {
                return a->categoryId() < b->categoryId();
            }
            return a->itemOrder() < b->itemOrder();
        });
        return sorted;
    }
} // namespace ui::screen::settings
