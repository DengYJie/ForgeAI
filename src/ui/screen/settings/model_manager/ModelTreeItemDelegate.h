#pragma once

#include <QStyledItemDelegate>
#include <QPoint>
#include <FluentQt/FluentQt.h>

#include <QPointer>

namespace fluent::collections {
    class TreeView;
}
namespace fluent::status_info {
    class ToolTip;
}

namespace ui::screen::settings::model_manager {

    enum ModelTreeRoles {
        IsGroupRole = Qt::UserRole + 10,
        ModelIdRole,
        ModelCapabilitiesRole,
        BrandIconRole
    };

    /**
     * @brief 模型树形视图委托渲染器
     * @details 负责绘制分组折叠卡片外观（带旋转 Chevron 箭头）以及模型行内的多色能力徽标（视觉/思考/工具）与操作按钮
     */
    class ModelTreeItemDelegate : public QStyledItemDelegate, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ModelTreeItemDelegate(fluent::collections::TreeView *treeView, QObject *parent = nullptr);
        ~ModelTreeItemDelegate() override;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    Q_SIGNALS:
        void modelSettingsRequested(const QString &modelId);
        void modelActionRequested(const QString &modelId);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        void showToolTip(const QString &text, const QRect &targetRect, QWidget *sourceWidget) const;
        void hideToolTip() const;

        fluent::collections::TreeView *m_treeView = nullptr;
        mutable QPointer<fluent::status_info::ToolTip> m_tooltip;
        mutable QString m_activeToolTipText;
        mutable QRect m_activeToolTipRect;
    };

} // namespace ui::screen::settings::model_manager
