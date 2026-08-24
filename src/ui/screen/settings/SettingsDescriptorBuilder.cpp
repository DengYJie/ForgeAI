#include "ui/screen/settings/SettingsDescriptorBuilder.h"

#include "ui/screen/settings/SettingsUIRegistry.h"
#include <QHash>
#include <algorithm>

namespace ui::screen::settings {

    QList<SettingsProviderPageDescriptor> SettingsDescriptorBuilder::buildDescriptors(const SettingsUIRegistry *uiRegistry) {
        if (!uiRegistry) return {};

        QHash<QString, SettingsProviderPageDescriptor> descriptorByProvider;
        const auto factories = uiRegistry->sortedFactories();
        for (const auto &factory : factories) {
            if (!factory) continue;
            const QString providerId = factory->providerId();
            if (providerId.isEmpty()) continue;

            auto it = descriptorByProvider.find(providerId);
            if (it == descriptorByProvider.end()) {
                SettingsProviderPageDescriptor descriptor;
                descriptor.providerId = providerId;
                descriptor.category = factory->categoryDisplayName();
                descriptor.categoryOrder = factory->categoryOrder();
                descriptor.title = factory->categoryDisplayName();
                descriptor.iconGlyph = factory->iconGlyph();
                descriptor.order = factory->categoryOrder();
                descriptor.factories.append(factory);
                descriptorByProvider.insert(providerId, descriptor);
            } else {
                if (factory->categoryOrder() < it->categoryOrder) {
                    it->categoryOrder = factory->categoryOrder();
                    it->category = factory->categoryDisplayName();
                    it->title = factory->categoryDisplayName();
                }
                if (!factory->iconGlyph().isEmpty() && it->iconGlyph.isEmpty()) {
                    it->iconGlyph = factory->iconGlyph();
                }
                it->factories.append(factory);
            }
        }

        const auto customPageFactories = uiRegistry->providerPageFactories();
        for (const auto &customFactory : customPageFactories) {
            if (!customFactory) continue;
            const QString providerId = customFactory->providerId();
            if (providerId.isEmpty()) continue;

            auto it = descriptorByProvider.find(providerId);
            if (it == descriptorByProvider.end()) {
                SettingsProviderPageDescriptor descriptor;
                descriptor.providerId = providerId;
                descriptor.category = customFactory->category();
                descriptor.categoryOrder = customFactory->categoryOrder();
                descriptor.title = customFactory->title();
                descriptor.iconGlyph = customFactory->iconGlyph();
                descriptor.order = customFactory->order();
                descriptor.pageFactory = customFactory;
                descriptorByProvider.insert(providerId, descriptor);
            } else {
                it->pageFactory = customFactory;
                it->category = customFactory->category();
                it->categoryOrder = customFactory->categoryOrder();
                it->title = customFactory->title();
                it->order = customFactory->order();
                if (!customFactory->iconGlyph().isEmpty()) {
                    it->iconGlyph = customFactory->iconGlyph();
                }
            }
        }

        auto descriptors = descriptorByProvider.values();
        std::stable_sort(descriptors.begin(), descriptors.end(),
                         [](const SettingsProviderPageDescriptor &a, const SettingsProviderPageDescriptor &b) {
                             if (a.categoryOrder != b.categoryOrder) return a.categoryOrder < b.categoryOrder;
                             if (a.order != b.order) return a.order < b.order;
                             return a.title < b.title;
                         });

        return descriptors;
    }

} // namespace ui::screen::settings
