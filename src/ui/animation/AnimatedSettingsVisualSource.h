#pragma once
#include "AnimatedVisualSource.h"
#include <QPainterPath>
#include <QtMath>
#include <cmath>

namespace ui::animation {
    /**
     * @brief 系统设置（齿轮）专属矢量动画源
     * 对标 WinUI 3 官方 AnimatedSettingsVisualSource (Controls_04_Settings)
     *
     * 在 Normal 状态下绘制 6 齿标准 Fluent 镂空设置齿轮；
     * 在 Pressed 状态下逆时针蓄力微转 -20°；
     * 在 Released 点击释放时以减速曲线顺时针飞速转动一整圈（360°）复位。
     */
    class AnimatedSettingsVisualSource : public AnimatedVisualSource {
    public:
        int duration(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) return anim.fast; // 150ms 快速逆时针蓄力
            if (from == IconState::Pressed) return 600; // 600ms 顺时针完整转动一圈
            return anim.normal;
        }

        QEasingCurve easing(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) {
                return anim.standard;
            }
            return anim.decelerate; // 飞转释放时使用减速曲线平滑刹车
        }

        void paint(QPainter &painter, const QRectF &rect,
                   IconState from, IconState to, qreal progress,
                   const fluent::FluentElement::Colors &colors, bool isEnabled) override {
            painter.setRenderHint(QPainter::Antialiasing);

            // WinUI 3 规范：图标颜色在 Hover 时不变色，统一使用 textPrimary（禁用时使用 textDisabled）
            const QColor currentColor = isEnabled ? colors.textPrimary : colors.textDisabled;

            // 计算当前旋转角度（度）
            qreal angle = 0.0;
            if (to == IconState::Pressed) {
                // 按下：逆时针蓄力微转 -20°
                angle = -20.0 * progress;
            } else if (from == IconState::Pressed) {
                // 释放：从 -20° 顺时针旋转一圈至 360° (0°)
                angle = -20.0 + (360.0 + 20.0) * progress;
            }

            const QPointF center = rect.center();
            painter.save();
            painter.translate(center);
            painter.rotate(angle);

            // 构造 100% 官方 WinUI 3 标准 Controls_04_Settings 6 齿镂空齿轮轮廓
            static const QPainterPath gearPath = createWinUI3GearPath();

            painter.setBrush(currentColor);
            painter.setPen(Qt::NoPen);
            painter.drawPath(gearPath);

            painter.restore();
        }

    private:
        static QPainterPath createWinUI3GearPath() {
            QPainterPath path;
            const qreal cx = 55.8590012;
            const qreal cy = 33.2389984;

            // 1. 外部 6 齿齿轮外边缘轮廓 (Geometry_1)
            path.moveTo(48.8800011 - cx, 30.9810009 - cy);
            path.cubicTo({49.1990013 - cx, 29.9960003 - cy}, {49.7229996 - cx, 29.0919991 - cy},
                         {50.4160004 - cx, 28.3250008 - cy});
            path.cubicTo({50.5330009 - cx, 28.1949997 - cy}, {50.7169991 - cx, 28.1490002 - cy},
                         {50.8810005 - cx, 28.2080002 - cy});
            path.lineTo(52.5359993 - cx, 28.7989998 - cy);
            path.cubicTo({52.9850006 - cx, 28.9589996 - cy}, {53.4790001 - cx, 28.7259998 - cy},
                         {53.6389999 - cx, 28.2770004 - cy});
            path.cubicTo({53.6549988 - cx, 28.2329998 - cy}, {53.6669998 - cx, 28.1879997 - cy},
                         {53.6749992 - cx, 28.1420002 - cy});
            path.lineTo(53.9900017 - cx, 26.4109993 - cy);
            path.cubicTo({54.0209999 - cx, 26.2390003 - cy}, {54.1539993 - cx, 26.1019993 - cy},
                         {54.3250008 - cx, 26.066 - cy});
            path.cubicTo({54.8260002 - cx, 25.9589996 - cy}, {55.3390007 - cx, 25.9060001 - cy},
                         {55.8590012 - cx, 25.9060001 - cy});
            path.cubicTo({56.3790016 - cx, 25.9060001 - cy}, {56.8919983 - cx, 25.9599991 - cy},
                         {57.3919983 - cx, 26.066 - cy});
            path.cubicTo({57.5629997 - cx, 26.1019993 - cy}, {57.6949997 - cx, 26.2390003 - cy},
                         {57.7260017 - cx, 26.4109993 - cy});
            path.lineTo(58.0419998 - cx, 28.1420002 - cy);
            path.cubicTo({58.1279984 - cx, 28.6110001 - cy}, {58.5769997 - cx, 28.9209995 - cy},
                         {59.0460014 - cx, 28.8360004 - cy});
            path.cubicTo({59.0919991 - cx, 28.8279991 - cy}, {59.137001 - cx, 28.8150005 - cy},
                         {59.1809998 - cx, 28.7989998 - cy});
            path.lineTo(60.8359985 - cx, 28.2080002 - cy);
            path.cubicTo({61.0 - cx, 28.1490002 - cy}, {61.1850014 - cx, 28.1949997 - cy},
                         {61.3019981 - cx, 28.3250008 - cy});
            path.cubicTo({61.9949989 - cx, 29.0919991 - cy}, {62.519001 - cx, 29.9960003 - cy},
                         {62.8380013 - cx, 30.9810009 - cy});
            path.cubicTo({62.8919983 - cx, 31.1469994 - cy}, {62.8390007 - cx, 31.3299999 - cy},
                         {62.7060013 - cx, 31.4430008 - cy});
            path.lineTo(61.3650017 - cx, 32.5810013 - cy);
            path.cubicTo({61.0019989 - cx, 32.8889999 - cy}, {60.9580002 - cx, 33.4350014 - cy},
                         {61.2659988 - cx, 33.7980003 - cy});
            path.cubicTo({61.2960014 - cx, 33.8339996 - cy}, {61.3289986 - cx, 33.8670006 - cy},
                         {61.3650017 - cx, 33.8969994 - cy});
            path.lineTo(62.7060013 - cx, 35.0359993 - cy);
            path.cubicTo({62.8390007 - cx, 35.1489983 - cy}, {62.8919983 - cx, 35.3310013 - cy},
                         {62.8380013 - cx, 35.4970016 - cy});
            path.cubicTo({62.519001 - cx, 36.4819984 - cy}, {61.9949989 - cx, 37.387001 - cy},
                         {61.3019981 - cx, 38.1539993 - cy});
            path.cubicTo({61.1850014 - cx, 38.2840004 - cy}, {61.0 - cx, 38.3300018 - cy},
                         {60.8359985 - cx, 38.2709999 - cy});
            path.lineTo(59.1809998 - cx, 37.6790009 - cy);
            path.cubicTo({58.7319984 - cx, 37.519001 - cy}, {58.2389984 - cx, 37.7519989 - cy},
                         {58.0789986 - cx, 38.2010002 - cy});
            path.cubicTo({58.0629997 - cx, 38.2449989 - cy}, {58.0499992 - cx, 38.2910004 - cy},
                         {58.0419998 - cx, 38.3370018 - cy});
            path.lineTo(57.7260017 - cx, 40.0680008 - cy);
            path.cubicTo({57.6949997 - cx, 40.2400017 - cy}, {57.5629997 - cx, 40.3759995 - cy},
                         {57.3919983 - cx, 40.4119987 - cy});
            path.cubicTo({56.8919983 - cx, 40.5180016 - cy}, {56.3790016 - cx, 40.5730019 - cy},
                         {55.8590012 - cx, 40.5730019 - cy});
            path.cubicTo({55.3390007 - cx, 40.5730019 - cy}, {54.8260002 - cx, 40.519001 - cy},
                         {54.3250008 - cx, 40.4119987 - cy});
            path.cubicTo({54.1539993 - cx, 40.3759995 - cy}, {54.0209999 - cx, 40.2389984 - cy},
                         {53.9900017 - cx, 40.0670013 - cy});
            path.lineTo(53.6749992 - cx, 38.3370018 - cy);
            path.cubicTo({53.5900002 - cx, 37.868 - cy}, {53.1409988 - cx, 37.5569992 - cy},
                         {52.6720009 - cx, 37.6430016 - cy});
            path.cubicTo({52.6259995 - cx, 37.651001 - cy}, {52.5800018 - cx, 37.6629982 - cy},
                         {52.5359993 - cx, 37.6790009 - cy});
            path.lineTo(50.8810005 - cx, 38.2709999 - cy);
            path.cubicTo({50.7169991 - cx, 38.3300018 - cy}, {50.5330009 - cx, 38.2840004 - cy},
                         {50.4160004 - cx, 38.1539993 - cy});
            path.cubicTo({49.7229996 - cx, 37.387001 - cy}, {49.1990013 - cx, 36.4819984 - cy},
                         {48.8800011 - cx, 35.4970016 - cy});
            path.cubicTo({48.8260002 - cx, 35.3310013 - cy}, {48.8779984 - cx, 35.1489983 - cy},
                         {49.0110016 - cx, 35.0359993 - cy});
            path.lineTo(50.3520012 - cx, 33.8969994 - cy);
            path.cubicTo({50.7150002 - cx, 33.5890007 - cy}, {50.7599983 - cx, 33.0439987 - cy},
                         {50.4519997 - cx, 32.6809998 - cy});
            path.cubicTo({50.4220009 - cx, 32.6450005 - cy}, {50.3880005 - cx, 32.6110001 - cy},
                         {50.3520012 - cx, 32.5810013 - cy});
            path.lineTo(49.0110016 - cx, 31.4430008 - cy);
            path.cubicTo({48.8779984 - cx, 31.3299999 - cy}, {48.8260002 - cx, 31.1469994 - cy},
                         {48.8800011 - cx, 30.9810009 - cy});
            path.closeSubpath();

            // 2. 外部 6 齿齿轮内边缘镂空轮廓 (Geometry_2)
            path.moveTo(49.7949982 - cx, 30.9759998 - cy);
            path.lineTo(50.9109993 - cx, 31.9239998 - cy);
            path.cubicTo({50.9819984 - cx, 31.9850006 - cy}, {51.0480003 - cx, 32.0509987 - cy},
                         {51.1090012 - cx, 32.1220016 - cy});
            path.cubicTo({51.7260017 - cx, 32.8479996 - cy}, {51.637001 - cx, 33.9370003 - cy},
                         {50.9109993 - cx, 34.5540009 - cy});
            path.lineTo(49.7949982 - cx, 35.5019989 - cy);
            path.cubicTo({50.0470009 - cx, 36.1769981 - cy}, {50.4099998 - cx, 36.8040009 - cy},
                         {50.8689995 - cx, 37.3590012 - cy});
            path.lineTo(52.2459984 - cx, 36.8670006 - cy);
            path.cubicTo({52.3339996 - cx, 36.8349991 - cy}, {52.4249992 - cx, 36.8110008 - cy},
                         {52.5169983 - cx, 36.7939987 - cy});
            path.cubicTo({53.4550018 - cx, 36.6230011 - cy}, {54.3530006 - cx, 37.2439995 - cy},
                         {54.5239983 - cx, 38.1819992 - cy});
            path.lineTo(54.7859993 - cx, 39.6209984 - cy);
            path.cubicTo({55.1380005 - cx, 39.6800003 - cy}, {55.4970016 - cx, 39.7099991 - cy},
                         {55.8590012 - cx, 39.7099991 - cy});
            path.cubicTo({56.2210007 - cx, 39.7099991 - cy}, {56.5789986 - cx, 39.6800003 - cy},
                         {56.9309998 - cx, 39.6209984 - cy});
            path.lineTo(57.1940002 - cx, 38.1819992 - cy);
            path.cubicTo({57.2109985 - cx, 38.0900002 - cy}, {57.2340012 - cx, 37.9990005 - cy},
                         {57.2659988 - cx, 37.9109993 - cy});
            path.cubicTo({57.5870018 - cx, 37.0139999 - cy}, {58.5740013 - cx, 36.5460014 - cy},
                         {59.4720001 - cx, 36.8670006 - cy});
            path.lineTo(60.848999 - cx, 37.3590012 - cy);
            path.cubicTo({61.3069992 - cx, 36.8040009 - cy}, {61.6710014 - cx, 36.1769981 - cy},
                         {61.9230003 - cx, 35.5019989 - cy});
            path.lineTo(60.8069992 - cx, 34.5540009 - cy);
            path.cubicTo({60.7360001 - cx, 34.493 - cy}, {60.6689987 - cx, 34.4269981 - cy},
                         {60.6080017 - cx, 34.355999 - cy});
            path.cubicTo({59.9910011 - cx, 33.6300011 - cy}, {60.0810013 - cx, 32.5410004 - cy},
                         {60.8069992 - cx, 31.9239998 - cy});
            path.lineTo(61.9230003 - cx, 30.9759998 - cy);
            path.cubicTo({61.6710014 - cx, 30.3010006 - cy}, {61.3069992 - cx, 29.6739998 - cy},
                         {60.848999 - cx, 29.1189995 - cy});
            path.lineTo(59.4720001 - cx, 29.6119995 - cy);
            path.cubicTo({59.3839989 - cx, 29.6439991 - cy}, {59.2929993 - cx, 29.6669998 - cy},
                         {59.2010002 - cx, 29.684 - cy});
            path.cubicTo({58.2639999 - cx, 29.8549995 - cy}, {57.3650017 - cx, 29.2339993 - cy},
                         {57.1940002 - cx, 28.2970009 - cy});
            path.lineTo(56.9309998 - cx, 26.8570004 - cy);
            path.cubicTo({56.5789986 - cx, 26.7980003 - cy}, {56.2210007 - cx, 26.7679996 - cy},
                         {55.8590012 - cx, 26.7679996 - cy});
            path.cubicTo({55.4970016 - cx, 26.7679996 - cy}, {55.1380005 - cx, 26.7980003 - cy},
                         {54.7859993 - cx, 26.8570004 - cy});
            path.lineTo(54.5239983 - cx, 28.2959995 - cy);
            path.cubicTo({54.507 - cx, 28.3880005 - cy}, {54.4830017 - cx, 28.4799995 - cy},
                         {54.4510002 - cx, 28.5680008 - cy});
            path.cubicTo({54.1300011 - cx, 29.4650002 - cy}, {53.1430016 - cx, 29.9330006 - cy},
                         {52.2459984 - cx, 29.6119995 - cy});
            path.lineTo(50.8689995 - cx, 29.1189995 - cy);
            path.cubicTo({50.4099998 - cx, 29.6739998 - cy}, {50.0470009 - cx, 30.3010006 - cy},
                         {49.7949982 - cx, 30.9759998 - cy});
            path.closeSubpath();

            // 3. 中心圆轴外圆 (Geometry_3) 与 内圆镂空 (Geometry_4)
            path.addEllipse(QPointF(0, 0), 2.157, 2.157);
            path.addEllipse(QPointF(0, 0), 1.294, 1.294);

            path.setFillRule(Qt::OddEvenFill);
            return path;
        }
    };
} // namespace ui::animation
