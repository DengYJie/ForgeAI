#pragma once

#include <QString>
#include "ui/navigation/NavigationWidget.h"

class QMouseEvent;

namespace ui::navigation {
    /**
 * @brief 导航分区标题项
 *
 * 用于视觉分组，左对齐于图标起始列，并在 Compact 模式下坍缩高度与淡出。
 */
    class NavigationSectionHeader : public NavigationWidget {
        Q_OBJECT

    public:
        explicit NavigationSectionHeader(const QString &text, QWidget *parent = nullptr);

        ~NavigationSectionHeader() override = default;

        QString text() const { return m_text; }

        void setCompacted(bool compacted) override;

        void setOrientation(Qt::Orientation orientation) override;

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;

        void mousePressEvent(QMouseEvent *event) override;

        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        void updateCompactHeight();

    private:
        QString m_text;
    };
} // namespace ui::navigation
