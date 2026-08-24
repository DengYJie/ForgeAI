#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QList>
#include <FluentQt/FluentQt.h>

#include "domain/model/ModelCapabilities.h"

namespace ui::widget::badge {

    /**
     * @brief 能力胶囊展示形态
     */
    enum class BadgeDisplayMode {
        IconOnly,     ///< 紧凑纯图标型 (28x20px, 如 [ 👁 ])
        IconAndText,  ///< 图标 + 文字型 (自适应宽度, 如 [ 👁 视觉 ])
        TextOnly      ///< 纯文字型 (自适应宽度, 如 [ 视觉 ])
    };

    struct CapabilityBadgeVisual {
        QColor bgColor;
        QColor fgColor;
        QString iconGlyph;
        QString displayName;
        QString tooltip;
    };

    /**
     * @brief 模型能力徽标样式与渲染器
     */
    class ModelCapabilityStyle {
    public:
        /**
         * @brief 获取指定能力的视觉参数常量引用（自动感知 FluentElement 宿主主题）
         */
        static const CapabilityBadgeVisual &visualRef(domain::model::ModelCapability cap,
                                                      const fluent::FluentElement *element);

        /**
         * @brief 获取指定能力的视觉参数常量引用（显式指定深浅主题）
         */
        static const CapabilityBadgeVisual &visualRef(domain::model::ModelCapability cap,
                                                      bool isDark);

        /**
         * @brief 计算指定模式下胶囊的推荐尺寸
         */
        static QSizeF sizeHintFor(domain::model::ModelCapability cap,
                                  BadgeDisplayMode mode = BadgeDisplayMode::IconOnly,
                                  const QFont &font = QFont());

        /**
         * @brief 高性能直绘能力胶囊（自动感知 FluentElement 宿主主题）
         */
        static void paintBadge(QPainter *painter, const QRectF &rect,
                               domain::model::ModelCapability cap,
                               const fluent::FluentElement *element,
                               BadgeDisplayMode mode = BadgeDisplayMode::IconOnly);

        /**
         * @brief 高性能直绘能力胶囊（显式指定深浅主题）
         */
        static void paintBadge(QPainter *painter, const QRectF &rect,
                               domain::model::ModelCapability cap,
                               bool isDark,
                               BadgeDisplayMode mode = BadgeDisplayMode::IconOnly);

        /**
         * @brief 获取适合在 UI 中展示胶囊 Badge 的高价值核心能力列表
         */
        static const QList<domain::model::ModelCapability> &featuredBadgeCapabilities();

        static constexpr QSizeF kDefaultBadgeSize{28.0, 20.0};
        static constexpr qreal kBadgeHeight = 20.0;
        static constexpr qreal kCornerRadius = 10.0;
    };

} // namespace ui::widget::badge
