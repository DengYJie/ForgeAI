#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>
#include <FluentQt/FluentQt.h>

namespace ui::screen::settings::model_manager {

    /**
     * @brief 分段式模型操作按钮
     * @details 左侧主区域触发“获取模型列表”，右侧分段区域触发“添加模型”
     */
    class ModelActionsSplitButton : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ModelActionsSplitButton(QWidget *parent = nullptr);
        ~ModelActionsSplitButton() override = default;

        void setRefreshing(bool refreshing);
        bool isRefreshing() const { return m_isRefreshing; }

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    Q_SIGNALS:
        void refreshRequested();
        void addRequested();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void onThemeUpdated() override;

    private:
        enum Part { None, Left, Right };
        Part hitTest(const QPoint &pos) const;

        bool m_isRefreshing = false;
        Part m_hoverPart = None;
        Part m_pressPart = None;

        const int m_rightWidth = 32;
    };

} // namespace ui::screen::settings::model_manager
