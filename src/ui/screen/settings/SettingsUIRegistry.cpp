#include "SettingsUIRegistry.h"

#include <algorithm>

namespace ui::screen::settings {
    void SettingsUIRegistry::registerFactory(std::shared_ptr<ISettingsUIFactory> factory) {
        if (factory) {
            m_factories.append(factory);
        }
    }

    void SettingsUIRegistry::registerProviderPageFactory(std::shared_ptr<ISettingsProviderPageFactory> factory) {
        if (factory) m_providerPageFactories.append(std::move(factory));
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

    std::shared_ptr<ISettingsProviderPageFactory> SettingsUIRegistry::providerPageFactory(const QString &providerId) const {
        for (const auto &factory : m_providerPageFactories) {
            if (factory && factory->providerId() == providerId) return factory;
        }
        return nullptr;
    }

    QList<std::shared_ptr<ISettingsProviderPageFactory>> SettingsUIRegistry::providerPageFactories() const {
        auto result = m_providerPageFactories;
        std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
            return a->categoryOrder() < b->categoryOrder();
        });
        return result;
    }
} // namespace ui::screen::settings
