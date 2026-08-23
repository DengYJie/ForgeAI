#pragma once

#include "../AbstractToolBlockWidget.h"
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

class QTextBrowser;

namespace ui::widget::message::blocks::tools {

/**
 * @brief 终端命令与输出卡片
 */
class BashToolWidget : public AbstractToolBlockWidget {
    Q_OBJECT
public:
    explicit BashToolWidget(QWidget* parent = nullptr);
    explicit BashToolWidget(const domain::agent::ToolCall& call, QWidget* parent = nullptr);
    ~BashToolWidget() override;

    void onThemeUpdated() override;

protected:
    QWidget* createContentWidget(QWidget* parent) override;
    void onCallUpdated(const domain::agent::ToolCall& call) override;
    void onResultUpdated(const domain::agent::ToolResult& result) override;
    QString customToolIcon(const QString& toolName) const override;

private:
    void updateVisualContent();
    QString colorizeCommandHtml(const QString& cmd, bool isDark) const;
    QString colorizeOutputHtml(const QString& output, bool isDark) const;

};

} // namespace ui::widget::message::blocks::tools
