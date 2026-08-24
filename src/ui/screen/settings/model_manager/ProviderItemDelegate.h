#pragma once

#include <QStyledItemDelegate>
#include <QPoint>
#include <FluentQt/FluentQt.h>

namespace ui::screen::settings::model_manager {

    /**
     * @brief 服务商列表项委托渲染器
     * @details 负责渲染服务商名称、默认态启停状态圆点与悬停态竖向三点菜单图标按钮
     */
    class ProviderItemDelegate : public QStyledItemDelegate, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderItemDelegate(QObject *parent = nullptr);
        ~ProviderItemDelegate() override = default;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    Q_SIGNALS:
        void menuRequested(const QString &providerId, const QPoint &globalPos);
    };

} // namespace ui::screen::settings::model_manager
