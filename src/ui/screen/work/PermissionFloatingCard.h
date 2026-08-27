#pragma once

#include <QWidget>
#include <QList>
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
}

namespace ui::screen::work {

class ToolPillBadge;
class ArgumentsCodeSurface;

/**
 * @brief 权限确认悬浮卡片 (Floating / Docked Permission Card)
 * @details 采用 Fluent 2 设计规范全自绘与 Token 驱动体系，呈现高危工具调用审批
 *          支持队列计数角标、Fluent 图标字形、工具标识胶囊与多范围授权 (Once, Run, Project, Deny)
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
    void permissionDecided(const QString& toolCallId, bool allow, domain::agent::PermissionScope scope);

protected:
    void paintEvent(QPaintEvent* event) override;
    void onThemeUpdated() override;

private:
    void setupUi();

    domain::agent::ToolCall m_currentCall;
    domain::agent::ToolPermission m_currentPermission;

    fluent::textfields::Label* m_headerTitleLabel = nullptr;
    ToolPillBadge* m_toolBadge = nullptr;
    fluent::textfields::Label* m_reasonLabel = nullptr;
    ArgumentsCodeSurface* m_argsSurface = nullptr;

    fluent::basicinput::Button* m_denyBtn = nullptr;
    fluent::basicinput::Button* m_allowOnceBtn = nullptr;
    fluent::basicinput::Button* m_allowRunBtn = nullptr;
    fluent::basicinput::Button* m_allowProjectBtn = nullptr;
};

} // namespace ui::screen::work
