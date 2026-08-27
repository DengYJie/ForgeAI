#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>

class QVBoxLayout;

namespace fluent::layout {
class Expander;
}

namespace ui::widget::message {

namespace blocks {
class FlatExpander;
}

/**
 * @brief 基于 FlatExpander 实现的执行过程折叠容器
 */
class ProcessGroupWidget : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    explicit ProcessGroupWidget(QWidget *parent = nullptr);
    ~ProcessGroupWidget() override;

    void setTitle(const QString &title);
    void setDurationMs(qint64 ms);
    void setExpanded(bool expanded, bool animated = true);
    bool isExpanded() const;

    // Add widgets (ThinkingBlock, ToolCard, etc.)
    void addProcessWidget(QWidget *widget);

    void onThemeUpdated() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void contentHeightChanged();

private slots:
    void onChildContentHeightChanged();

private:
    blocks::FlatExpander *m_expander = nullptr;
    QWidget *m_contentContainer = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QString m_baseTitle = QStringLiteral("已处理");
};

} // namespace ui::widget::message
