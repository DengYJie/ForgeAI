#pragma once

#include <QModelIndex>
#include <QList>
#include <FluentQt/Collections.h>

namespace ui::widget::tree {

    class AutoHeightTreeView : public fluent::collections::TreeView {
        Q_OBJECT

    public:
        explicit AutoHeightTreeView(QWidget *parent = nullptr);

        void setModel(QAbstractItemModel *model) override;

        int emptyHeight() const { return m_emptyHeight; }
        void setEmptyHeight(int height) { m_emptyHeight = height; scheduleHeightUpdate(); }

    public Q_SLOTS:
        void scheduleHeightUpdate();
        void updateContentHeight();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void onThemeUpdated() override;

    private:
        QList<QModelIndex> visibleIndexes() const;
        int calculateContentHeight() const;

        int m_emptyHeight = 100;
        bool m_heightUpdatePending = false;
    };

} // namespace ui::widget::tree
