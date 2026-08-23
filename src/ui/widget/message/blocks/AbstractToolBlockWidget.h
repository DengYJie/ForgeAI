#pragma once

#include "domain/agent/ToolExecution.h"
#include <FluentQt/Foundation.h>
#include <QWidget>

namespace ui::widget::message::blocks {

class FlatExpander;

} // namespace ui::widget::message::blocks

namespace fluent::layout { class Card; }
namespace fluent::textfields { class Label; }
class QTextBrowser;

namespace ui::widget::message::blocks {

/**
 * @brief 工具调用抽象基类卡片（统一承载 FlatExpander 折叠容器、状态徽标、图标与高度级联）
 */
class AbstractToolBlockWidget : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    enum class Status {
        Running,   ///< 正在执行
        Success,   ///< 成功完成
        Error      ///< 执行失败
    };

    explicit AbstractToolBlockWidget(QWidget* parent = nullptr);
    explicit AbstractToolBlockWidget(const domain::agent::ToolCall& call, QWidget* parent = nullptr);
    ~AbstractToolBlockWidget() override;

    const QString& toolCallId() const { return m_call.id; }
    const domain::agent::ToolCall& toolCall() const { return m_call; }
    const domain::agent::ToolResult& toolResult() const { return m_result; }
    Status status() const { return m_status; }

    void setToolCall(const domain::agent::ToolCall& call);
    void setToolResult(const domain::agent::ToolResult& result);
    void setStatus(Status status);

    void setExpanded(bool expanded, bool animated = true);
    bool isExpanded() const;

    void onThemeUpdated() override;

signals:
    void contentHeightChanged();

protected:
    /// 子类必须实现：创建卡片内部专属的视觉内容组件
    virtual QWidget* createContentWidget(QWidget* parent) = 0;
    /// 子类实现：当 ToolCall 参数变化时的更新回调
    virtual void onCallUpdated(const domain::agent::ToolCall& call) = 0;
    /// 子类实现：当 ToolResult 结果产出时的更新回调
    virtual void onResultUpdated(const domain::agent::ToolResult& result) = 0;
    /// 子类可重写：自定义头部图标
    virtual QString customToolIcon(const QString& toolName) const;
    /// 子类可重写：自定义标题文本
    virtual QString customToolTitle(const QString& name, const QString& arguments) const;

    void initBaseUi();
    void updateHeader();

    domain::agent::ToolCall m_call;
    domain::agent::ToolResult m_result;
    Status m_status = Status::Running;

    FlatExpander* m_expander = nullptr;
    QWidget* m_contentWidget = nullptr;

    // --- 标准卡片复用组件 (Decoupled UI components for tool cards) ---
    fluent::layout::Card* m_cardSurface = nullptr;
    fluent::textfields::Label* m_section1Label = nullptr;
    fluent::layout::Card* m_section1Card = nullptr;
    fluent::textfields::Label* m_section1Text = nullptr;

    fluent::textfields::Label* m_section2Label = nullptr;
    QTextBrowser* m_section2Browser = nullptr;

    /// 统一创建标准两段式（参数/输出）的卡片内容面板
    QWidget* createStandardContentWidget(QWidget* parent, const QString& section1Title, const QString& section2Title);
    /// 更新第一部分（如：命令、文件）的内容及高度调整
    void updateStandardSection1(const QString& html);
    /// 更新第二部分（如：输出、内容）的内容及高度调整
    void updateStandardSection2(const QString& html);
};

} // namespace ui::widget::message::blocks
