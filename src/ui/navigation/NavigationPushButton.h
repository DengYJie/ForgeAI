#pragma once

#include "ui/navigation/NavigationWidget.h"
#include <QString>
#include <memory>

namespace ui::animation {
    class AnimatedVisualSource;
    class AnimatedIcon;
}

namespace ui::navigation {
    /**
 * @brief 基础图文导航按钮
 *
 * 实现标准的 Fluent 图标+文字渲染、悬停背景及折叠过渡中的缩进插值。
 */
    class NavigationPushButton : public NavigationWidget {
        Q_OBJECT

    public:
        explicit NavigationPushButton(const QString &iconGlyph, const QString &text,
                                      bool isSelectable = true, QWidget *parent = nullptr);

        ~NavigationPushButton() override = default;

        QString text() const { return m_text; }

        void setText(const QString &text);

        QString iconGlyph() const { return m_iconGlyph; }

        void setIconGlyph(const QString &glyph);

        /**
     * @brief 获取当前挂载的动态矢量动画源
     * @return 矢量动画源指针，未设置时为 nullptr
     */
        std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource() const { return m_visualSource; }

        /**
     * @brief 设置动态矢量动画源
     * @param source 矢量动画源智能指针
     */
        void setVisualSource(std::shared_ptr<ui::animation::AnimatedVisualSource> source);

        int iconSize() const { return m_iconSize; }

        void setIconSize(int size);

        QSize sizeHint() const override;

        /**
     * @brief 更新矢量动画图标组件的几何对齐坐标
     */
        void updateIconGeometry();

    protected:
        void paintEvent(QPaintEvent *event) override;

        void resizeEvent(QResizeEvent *event) override;

        /// 随折叠进度消除子项缩进以契合 48px 紧凑栏
        virtual int iconDrawX() const;

        /// 允许子类为右侧元素（如展开箭头、未读红点）预留文本排版空间
        virtual int textRightOffset() const { return 0; }

    protected:
        QString m_iconGlyph;
        QString m_text;
        int m_iconSize = Typography::IconSize::Standard;
        std::shared_ptr<ui::animation::AnimatedVisualSource> m_visualSource;
        ui::animation::AnimatedIcon *m_animatedIcon = nullptr;
    };
} // namespace ui::navigation
