#pragma once

#include "../AbstractToolBlockWidget.h"
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

class QTextBrowser;

namespace ui::widget::message::blocks::tools {

/**
 * @brief 通用工具展示卡片
 */
class GenericToolWidget : public AbstractToolBlockWidget {
    Q_OBJECT
public:
    explicit GenericToolWidget(QWidget* parent = nullptr);
    explicit GenericToolWidget(const domain::agent::ToolCall& call, QWidget* parent = nullptr);
    ~GenericToolWidget() override;

    void onThemeUpdated() override;

protected:
    QWidget* createContentWidget(QWidget* parent) override;
    void onCallUpdated(const domain::agent::ToolCall& call) override;
    void onResultUpdated(const domain::agent::ToolResult& result) override;

private:
    void updateVisualContent();

};

} // namespace ui::widget::message::blocks::tools
