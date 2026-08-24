#include "ModelCapabilityStyle.h"

#include <QPainterPath>
#include <QFontMetrics>
#include <QHash>
#include <FluentQt/Design.h>

namespace ui::widget::badge {

    using domain::model::ModelCapability;

    namespace {

        QHash<ModelCapability, CapabilityBadgeVisual> buildPalette(bool isDark) {
            QHash<ModelCapability, CapabilityBadgeVisual> map;

            // 1. Vision (视觉多模态)
            map.insert(ModelCapability::Vision, {
                isDark ? QColor(18, 56, 41) : QColor(230, 248, 240),
                isDark ? QColor(52, 199, 89) : QColor(15, 157, 88),
                Typography::Icons::View,
                QObject::tr("视觉"),
                QObject::tr("支持图像与多模态视觉输入")
            });

            // 2. Thinking (深度思考/推理)
            map.insert(ModelCapability::Thinking, {
                isDark ? QColor(45, 28, 77) : QColor(242, 232, 255),
                isDark ? QColor(175, 82, 222) : QColor(124, 58, 237),
                Typography::Icons::Brightness,
                QObject::tr("深度思考"),
                QObject::tr("支持深度推理与思考过程展示")
            });

            // 3. ToolCalling (工具与函数调用)
            map.insert(ModelCapability::ToolCalling, {
                isDark ? QColor(62, 39, 20) : QColor(255, 241, 229),
                isDark ? QColor(255, 149, 0) : QColor(216, 100, 0),
                Typography::Icons::Edit,
                QObject::tr("工具调用"),
                QObject::tr("支持原生函数与工具调用 (Tool / Function Calling)")
            });

            // 4. Audio (语音对话)
            map.insert(ModelCapability::Audio, {
                isDark ? QColor(10, 40, 70) : QColor(235, 248, 255),
                isDark ? QColor(10, 132, 255) : QColor(0, 122, 255),
                Typography::Icons::Microphone,
                QObject::tr("语音对话"),
                QObject::tr("支持音频输入与语音多模态对话")
            });

            // 5. Video (视频理解)
            map.insert(ModelCapability::Video, {
                isDark ? QColor(70, 15, 30) : QColor(255, 235, 240),
                isDark ? QColor(255, 45, 85) : QColor(225, 29, 72),
                Typography::Icons::Video,
                QObject::tr("视频理解"),
                QObject::tr("支持视频内容多模态解析")
            });

            // 6. Pdf (长文档解析)
            map.insert(ModelCapability::Pdf, {
                isDark ? QColor(60, 40, 10) : QColor(254, 243, 199),
                isDark ? QColor(245, 158, 11) : QColor(180, 83, 9),
                Typography::Icons::Document,
                QObject::tr("文档解析"),
                QObject::tr("原生支持 PDF 等长文档解析")
            });

            return map;
        }

        const QHash<ModelCapability, CapabilityBadgeVisual> &paletteFor(bool isDark) {
            static const auto s_lightPalette = buildPalette(false);
            static const auto s_darkPalette = buildPalette(true);
            return isDark ? s_darkPalette : s_lightPalette;
        }

    } // namespace

    const CapabilityBadgeVisual &ModelCapabilityStyle::visualRef(ModelCapability cap, bool isDark) {
        static const CapabilityBadgeVisual s_empty{};
        const auto &palette = paletteFor(isDark);
        const auto it = palette.constFind(cap);
        return (it != palette.constEnd()) ? it.value() : s_empty;
    }

    const CapabilityBadgeVisual &ModelCapabilityStyle::visualRef(ModelCapability cap,
                                                                  const fluent::FluentElement *element) {
        const bool isDark = element && (element->effectiveTheme() == fluent::FluentElement::Dark);
        return visualRef(cap, isDark);
    }

    QSizeF ModelCapabilityStyle::sizeHintFor(ModelCapability cap, BadgeDisplayMode mode, const QFont &font) {
        if (mode == BadgeDisplayMode::IconOnly) {
            return QSizeF(28.0, kBadgeHeight);
        }

        const auto &visual = visualRef(cap, false);
        if (visual.displayName.isEmpty()) {
            return QSizeF(28.0, kBadgeHeight);
        }

        QFont textFont = font.family().isEmpty()
                             ? Typography::fontStyle(Typography::FontRole::Caption).toQFont()
                             : font;
        QFontMetrics fm(textFont);
        const int textW = fm.horizontalAdvance(visual.displayName);

        if (mode == BadgeDisplayMode::IconAndText) {
            return QSizeF(textW + 30.0, kBadgeHeight);
        }
        return QSizeF(textW + 16.0, kBadgeHeight);
    }

    void ModelCapabilityStyle::paintBadge(QPainter *painter, const QRectF &rect,
                                          ModelCapability cap, bool isDark,
                                          BadgeDisplayMode mode) {
        const auto &visual = visualRef(cap, isDark);
        if (visual.iconGlyph.isEmpty()) return;

        QPainterPath pillPath;
        pillPath.addRoundedRect(rect, kCornerRadius, kCornerRadius);
        painter->fillPath(pillPath, visual.bgColor);

        if (mode == BadgeDisplayMode::IconOnly) {
            QFont iconFont(Typography::FontFamily::FluentIcons);
            iconFont.setPixelSize(Typography::IconSize::Compact);
            painter->setFont(iconFont);
            painter->setPen(visual.fgColor);
            painter->drawText(rect, Qt::AlignCenter, visual.iconGlyph);
        }
        else if (mode == BadgeDisplayMode::IconAndText) {
            // 左侧图标
            QFont iconFont(Typography::FontFamily::FluentIcons);
            iconFont.setPixelSize(Typography::IconSize::Compact);
            painter->setFont(iconFont);
            painter->setPen(visual.fgColor);
            const QRectF iconRect(rect.left() + 7.0, rect.top(), 14.0, rect.height());
            painter->drawText(iconRect, Qt::AlignCenter, visual.iconGlyph);

            // 右侧文本
            QFont textFont = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
            painter->setFont(textFont);
            const QRectF textRect(rect.left() + 23.0, rect.top(), qMax(0.0, rect.width() - 30.0), rect.height());
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, visual.displayName);
        }
        else if (mode == BadgeDisplayMode::TextOnly) {
            QFont textFont = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
            painter->setFont(textFont);
            painter->setPen(visual.fgColor);
            painter->drawText(rect, Qt::AlignCenter, visual.displayName);
        }
    }

    void ModelCapabilityStyle::paintBadge(QPainter *painter, const QRectF &rect,
                                          ModelCapability cap,
                                          const fluent::FluentElement *element,
                                          BadgeDisplayMode mode) {
        const bool isDark = element && (element->effectiveTheme() == fluent::FluentElement::Dark);
        paintBadge(painter, rect, cap, isDark, mode);
    }

    const QList<ModelCapability> &ModelCapabilityStyle::featuredBadgeCapabilities() {
        static const QList<ModelCapability> s_featured = {
            ModelCapability::Vision,
            ModelCapability::Thinking,
            ModelCapability::ToolCalling,
            ModelCapability::Audio,
            ModelCapability::Video,
            ModelCapability::Pdf
        };
        return s_featured;
    }

} // namespace ui::widget::badge
