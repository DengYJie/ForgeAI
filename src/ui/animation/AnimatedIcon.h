#pragma once
#include "AnimatedVisualSource.h"
#include <FluentQt/Foundation.h>
#include <memory>
#include <QWidget>

class QVariantAnimation;

namespace ui::animation {
    /**
     * @brief 支持矢量状态动画的图标容器组件
     *
     * 配合 AnimatedVisualSource，实现图标按状态平滑过渡的微形变动画。
     */
    class AnimatedIcon : public QWidget, public fluent::FluentElement {
        Q_OBJECT
        Q_PROPERTY(qreal progress READ progress WRITE setProgress)

    public:
        explicit AnimatedIcon(QWidget *parent = nullptr);

        ~AnimatedIcon() override;

        void onThemeUpdated() override { update(); }

        /**
         * @brief 设置动画视觉源
         * @param source 矢量动画源实例
         */
        void setSource(std::shared_ptr<AnimatedVisualSource> source);

        /**
         * @brief 触发状态切换并执行对应过渡动画
         * @param newState 目标状态
         */
        void setState(IconState newState);

        /**
         * @brief 获取当前图标状态
         * @return 当前状态
         */
        IconState state() const { return m_currentState; }

        /**
         * @brief 获取当前动画进度
         * @return 进度 [0.0, 1.0]
         */
        qreal progress() const;

        /**
         * @brief 设置当前动画进度
         * @param p 进度 [0.0, 1.0]
         */
        void setProgress(qreal p);

        /**
         * @brief 设置是否随父容器自动铺满尺寸
         * @param autoFill 若为 true 则在父容器 resize 时保持填满，false 则由外部控制几何位置
         */
        void setAutoFillParent(bool autoFill) { m_autoFillParent = autoFill; }

        /**
         * @brief 获取是否随父容器自动铺满尺寸
         * @return 是否自动填满父容器
         */
        bool autoFillParent() const { return m_autoFillParent; }

    protected:
        void paintEvent(QPaintEvent *event) override;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        std::shared_ptr<AnimatedVisualSource> m_source;
        IconState m_previousState = IconState::Normal;
        IconState m_currentState = IconState::Normal;
        qreal m_progress = 1.0;
        bool m_autoFillParent = true;
        QVariantAnimation *m_animation = nullptr;
    };
} // namespace ui::animation
