#pragma once

#include <QWidget>
#include <QList>
#include <QButtonGroup>
#include "domain/agent/AgentPolicy.h"
#include "domain/agent/AgentRunState.h"
#include <components/foundation/FluentElement.h>

class QVBoxLayout;
class QHBoxLayout;

namespace fluent::basicinput {
    class Button;
}
namespace fluent::textfields {
    class Label;
    class LineEdit;
}

namespace ui::screen::work {

class ToolPillBadge;
class ArgumentsCodeSurface;
class ScopeRadioRow;

/**
 * @brief 权限确认悬浮卡片 (Floating / Docked Permission Card)
 * @details 采用 Fluent 2 设计规范全自绘与 Token 驱动体系，呈现高危工具与终端命令调用审批。
 *          提供 5 项垂直选项（仅一次、本对话、本项目、永远、自定义输入框）与底部 [Skip] / [Approve] 操作。
 */
class PermissionFloatingCard : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit PermissionFloatingCard(QWidget* parent = nullptr);
    ~PermissionFloatingCard() override = default;

    void setPermission(
        const domain::agent::ToolCall& call,
        const domain::agent::ToolPermission& permission,
        int currentIndex,
        int totalCount
    );

    QString currentToolCallId() const { return m_currentCall.id; }

signals:
    void permissionDecided(
        const QString& toolCallId,
        bool allow,
        domain::agent::PermissionScope scope,
        const QString& customInput
    );

protected:
    void paintEvent(QPaintEvent* event) override;
    void onThemeUpdated() override;

private:
    void setupUi();
    void selectOption(int index);
    void triggerApprove();
    void triggerSkip();

    domain::agent::ToolCall m_currentCall;
    domain::agent::ToolPermission m_currentPermission;

    fluent::textfields::Label* m_headerTitleLabel = nullptr;
    ToolPillBadge* m_toolBadge = nullptr;
    fluent::textfields::Label* m_reasonLabel = nullptr;
    ArgumentsCodeSurface* m_argsSurface = nullptr;

    // 5 项垂直选项组件
    int m_selectedOptionIndex = 0; // 0: Once, 1: Run, 2: Project, 3: Global, 4: CustomInput
    QList<ScopeRadioRow*> m_radioRows;
    fluent::textfields::LineEdit* m_customInputEdit = nullptr;

    // 底部动作按钮
    fluent::basicinput::Button* m_skipBtn = nullptr;
    fluent::basicinput::Button* m_approveBtn = nullptr;
};

} // namespace ui::screen::work
