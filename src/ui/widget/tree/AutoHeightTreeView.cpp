#include "AutoHeightTreeView.h"

#include <QResizeEvent>
#include <QTimer>
#include <climits>

namespace ui::widget::tree {

    AutoHeightTreeView::AutoHeightTreeView(QWidget *parent)
        : fluent::collections::TreeView(parent) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setScrollChainingEnabled(true);

        connect(this, &QTreeView::expanded, this, &AutoHeightTreeView::scheduleHeightUpdate);
        connect(this, &QTreeView::collapsed, this, &AutoHeightTreeView::scheduleHeightUpdate);
    }

    void AutoHeightTreeView::setModel(QAbstractItemModel *model) {
        if (this->model()) {
            disconnect(this->model(), nullptr, this, nullptr);
        }

        fluent::collections::TreeView::setModel(model);

        if (model) {
            connect(model, &QAbstractItemModel::modelReset, this, &AutoHeightTreeView::scheduleHeightUpdate);
            connect(model, &QAbstractItemModel::rowsInserted, this, &AutoHeightTreeView::scheduleHeightUpdate);
            connect(model, &QAbstractItemModel::rowsRemoved, this, &AutoHeightTreeView::scheduleHeightUpdate);
            connect(model, &QAbstractItemModel::layoutChanged, this, &AutoHeightTreeView::scheduleHeightUpdate);
            connect(model, &QAbstractItemModel::dataChanged, this, &AutoHeightTreeView::scheduleHeightUpdate);
        }

        scheduleHeightUpdate();
    }

    void AutoHeightTreeView::scheduleHeightUpdate() {
        if (m_heightUpdatePending) return;
        m_heightUpdatePending = true;

        QTimer::singleShot(0, this, [this] {
            m_heightUpdatePending = false;
            updateContentHeight();
        });
    }

    void AutoHeightTreeView::updateContentHeight() {
        doItemsLayout();

        const int targetHeight = calculateContentHeight();
        if (qAbs(targetHeight - height()) <= 1) return;

        setFixedHeight(targetHeight);
        updateGeometry();
    }

    void AutoHeightTreeView::resizeEvent(QResizeEvent *event) {
        fluent::collections::TreeView::resizeEvent(event);
        if (event->size().width() != event->oldSize().width()) {
            doItemsLayout();
            scheduleHeightUpdate();
        }
    }

    void AutoHeightTreeView::onThemeUpdated() {
        fluent::collections::TreeView::onThemeUpdated();
        doItemsLayout();
        scheduleHeightUpdate();
    }

    QList<QModelIndex> AutoHeightTreeView::visibleIndexes() const {
        QList<QModelIndex> result;
        if (!model()) return result;

        const int rootCount = model()->rowCount();
        for (int row = 0; row < rootCount; ++row) {
            const QModelIndex root = model()->index(row, 0);
            if (!root.isValid()) continue;

            result.push_back(root);

            if (isExpanded(root)) {
                const int childCount = model()->rowCount(root);
                for (int childRow = 0; childRow < childCount; ++childRow) {
                    const QModelIndex child = model()->index(childRow, 0, root);
                    if (child.isValid()) {
                        result.push_back(child);
                    }
                }
            }
        }
        return result;
    }

    int AutoHeightTreeView::calculateContentHeight() const {
        if (!model() || model()->rowCount() == 0) {
            return m_emptyHeight;
        }

        const auto indexes = visibleIndexes();
        if (indexes.isEmpty()) {
            return m_emptyHeight;
        }

        int top = INT_MAX;
        int bottom = 0;

        for (const QModelIndex &index : indexes) {
            const QRect rect = visualRect(index);
            if (rect.isValid() && rect.height() > 0) {
                top = qMin(top, rect.top());
                bottom = qMax(bottom, rect.bottom() + 1);
            }
        }

        if (top == INT_MAX) {
            int fallbackHeight = 0;
            for (const auto &idx : indexes) {
                fallbackHeight += qMax(0, sizeHintForIndex(idx).height());
            }
            return qMax(m_emptyHeight, fallbackHeight);
        }

        int total = bottom - top;
        const QMargins margins = viewportMargins();
        total += margins.top() + margins.bottom();
        total += frameWidth() * 2;

        return qMax(m_emptyHeight, total);
    }

} // namespace ui::widget::tree
