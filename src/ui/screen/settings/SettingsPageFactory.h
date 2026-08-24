#pragma once

#include <QWidget>
#include <QList>
#include "ui/screen/settings/SettingsDescriptorBuilder.h"

class QBoxLayout;

namespace ui::screen::settings {

    /**
     * @brief 设置页面构建工厂
     * @details 负责将 SettingsProviderPageDescriptor 转换为实际的 QWidget 页面或延迟加载容器
     */
    class SettingsPageFactory {
    public:
        static QWidget *createLazyPage(
            const SettingsProviderPageDescriptor &descriptor,
            QList<QBoxLayout *> &pageLayouts,
            QList<QWidget *> &cards,
            QWidget *parent = nullptr
        );

        static QWidget *createGenericProviderPage(
            const SettingsProviderPageDescriptor &descriptor,
            QList<QBoxLayout *> &pageLayouts,
            QList<QWidget *> &cards,
            QWidget *parent = nullptr
        );

        static QWidget *createCustomProviderPage(
            const SettingsProviderPageDescriptor &descriptor,
            QWidget *parent = nullptr
        );

        static QWidget *createSectionHeader(const QString &title, QWidget *parent = nullptr);

        static QWidget *createSettingsCard(
            const QString &iconGlyph,
            const QString &title,
            const QString &subtitle,
            QWidget *trailingWidget,
            QWidget *parent = nullptr
        );
    };

} // namespace ui::screen::settings
