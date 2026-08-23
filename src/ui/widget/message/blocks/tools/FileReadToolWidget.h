#pragma once

#include "../AbstractToolBlockWidget.h"
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

class QTextBrowser;

namespace ui::widget::message::blocks::tools {

/**
 * @brief 文件读取与预览卡片
 */
class FileReadToolWidget : public AbstractToolBlockWidget {
    Q_OBJECT
public:
    explicit FileReadToolWidget(QWidget* parent = nullptr);
    explicit FileReadToolWidget(const domain::agent::ToolCall& call, QWidget* parent = nullptr);
    ~FileReadToolWidget() override;

    void onThemeUpdated() override;

protected:
    QWidget* createContentWidget(QWidget* parent) override;
    void onCallUpdated(const domain::agent::ToolCall& call) override;
    void onResultUpdated(const domain::agent::ToolResult& result) override;
    QString customToolIcon(const QString& toolName) const override;
    QString customToolTitle(const QString& name, const QString& arguments) const override;

private:
    void updateVisualContent();

};

} // namespace ui::widget::message::blocks::tools
