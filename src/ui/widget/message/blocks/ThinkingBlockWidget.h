#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>

namespace fluent::layout {
class Expander;
}

namespace ui::widget {
class MarkdownView;
}

namespace ui::widget::message::blocks {

class FlatExpander;

/**
 * @brief 思考链展示块（基于 FlatExpander 实现平滑折叠与耗时展示，仿 Cherry Studio ThinkingBlock）
 */
class ThinkingBlockWidget : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    explicit ThinkingBlockWidget(QWidget *parent = nullptr);
    ~ThinkingBlockWidget() override;

    void setThought(const QString &thought);
    void appendThought(const QString &chunk);
    void setDurationMs(qint64 ms);
    void setMaxHeight(int maxH);
    int maxHeight() const { return m_maxHeight; }
    void setExpanded(bool expanded, bool animated = true);
    bool isExpanded() const;
    void beginStream();
    void finishStream();
    bool isStreaming() const;

    void onThemeUpdated() override;

signals:
    void contentHeightChanged();

private:
    void setupUi();
    void updateTitle();

    FlatExpander *m_expander = nullptr;
    ui::widget::MarkdownView *m_markdownView = nullptr;
    QString m_thoughtText;
    qint64 m_durationMs = 0;
    int m_maxHeight = 300;
};

} // namespace ui::widget::message::blocks
