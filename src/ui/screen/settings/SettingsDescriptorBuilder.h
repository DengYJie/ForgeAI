#pragma once

#include <QList>
#include <QString>
#include <memory>
#include "ui/screen/settings/ISettingsUIFactory.h"

namespace ui::screen::settings {

    class SettingsUIRegistry;

    /**
     * @brief 设置提供者页面描述符
     */
    struct SettingsProviderPageDescriptor {
        QString providerId;
        QString category;
        QString title;
        QString iconGlyph;
        int categoryOrder = 1000;
        int order = 1000;
        QList<std::shared_ptr<ISettingsUIFactory>> factories;
        std::shared_ptr<ISettingsProviderPageFactory> pageFactory;

        bool hasCustomPage() const { return pageFactory != nullptr; }
    };

    /**
     * @brief 设置描述符构建器
     * @details 负责从 SettingsUIRegistry 提取、归类、排序并生成设置提供者页面描述符列表
     */
    class SettingsDescriptorBuilder {
    public:
        static QList<SettingsProviderPageDescriptor> buildDescriptors(const SettingsUIRegistry *uiRegistry);
    };

} // namespace ui::screen::settings
