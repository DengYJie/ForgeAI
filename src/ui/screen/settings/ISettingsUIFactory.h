#pragma once
#include <QString>
#include <QWidget>
#include <memory>

namespace ui::screen::settings {
    /**
     * @brief 自定义设置提供者页面工厂抽象接口
     * @details 为一个 Provider 提供完整的自定义设置页面及导航展示元数据
     */
    class ISettingsProviderPageFactory {
    public:
        virtual ~ISettingsProviderPageFactory() = default;

        /** 所属 SettingsProvider 的稳定 ID。 */
        virtual QString providerId() const = 0;

        /** 获取所属分类显示名称。 */
        virtual QString category() const = 0;

        /** 获取设置页面显示标题。 */
        virtual QString title() const = 0;

        /** 获取分类排序权重（数值越小越靠前）。 */
        virtual int categoryOrder() const { return 0; }

        /** 获取项排序权重（数值越小越靠前）。 */
        virtual int order() const { return 0; }

        /** 获取导航图标字形码。 */
        virtual QString iconGlyph() const = 0;

        /** 创建自定义设置页面控件。 */
        virtual QWidget *createProviderPage(QWidget *parent) = 0;
    };

    /**
     * @brief 设置卡片 UI 工厂抽象接口
     * @details 负责提供单个设置项或设置组的元数据描述（标题、副标题、图标、分类、排序），并负责创建对应的交互控件
     */
    class ISettingsUIFactory {
    public:
        virtual ~ISettingsUIFactory() = default;

        /**
         * @brief 获取工厂唯一标识符
         * @return 稳定唯一的 Factory ID (如 "model.manager", "appearance.theme", "logging.level")
         */
        virtual QString id() const = 0;

        /** 所属 SettingsProvider 的稳定 ID。 */
        virtual QString providerId() const = 0;

        /**
         * @brief 获取分类标识符（机器可读稳定 ID，如 "model", "appearance", "diagnostics"）
         * @return 分类 ID
         */
        virtual QString categoryId() const = 0;

        /**
         * @brief 获取分类显示名称（用于 SettingsNavigationModule 界面分组标题呈现）
         * @return 本地化的分类显示文本
         */
        virtual QString categoryDisplayName() const = 0;

        /**
         * @brief 获取分类全局显示顺序
         * @return 排序数值（数值越小越靠前渲染）
         */
        virtual int categoryOrder() const { return 0; }

        /**
         * @brief 获取卡片项在所属分类内部的显示顺序
         * @return 排序数值（数值越小越靠前渲染）
         */
        virtual int itemOrder() const { return 0; }

        /**
         * @brief 获取卡片图标字形码 (FontIcon glyph)
         * @return 图标标识
         */
        virtual QString iconGlyph() const = 0;

        /**
         * @brief 获取设置项标题
         * @return 本地化主标题文本
         */
        virtual QString title() const = 0;

        /**
         * @brief 获取设置项详细副标题或描述说明
         * @return 本地化说明文本
         */
        virtual QString subtitle() const = 0;

        /**
         * @brief 创建具体的设置项交互控件
         * @param parent 宿主父控件
         * @return 构造出的控件指针
         */
        virtual QWidget *createControlWidget(QWidget *parent) = 0;
    };
} // namespace ui::screen::settings
