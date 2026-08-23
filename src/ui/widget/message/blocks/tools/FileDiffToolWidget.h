#pragma once

#include "../AbstractToolBlockWidget.h"
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

class QTextBrowser;

namespace ui::widget::message::blocks::tools {

/**
 * @brief 文件差异修改卡片
 */
class FileDiffToolWidget : public AbstractToolBlockWidget {
    Q_OBJECT
public:
    explicit FileDiffToolWidget(QWidget* parent = nullptr);
    explicit FileDiffToolWidget(const domain::agent::ToolCall& call, QWidget* parent = nullptr);
    ~FileDiffToolWidget() override;

    void onThemeUpdated() override;

protected:
    QWidget* createContentWidget(QWidget* parent) override;
    void onCallUpdated(const domain::agent::ToolCall& call) override;
    void onResultUpdated(const domain::agent::ToolResult& result) override;
    QString customToolIcon(const QString& toolName) const override;
    QString customToolTitle(const QString& name, const QString& arguments) const override;

private:
    void updateVisualContent();
    QString colorizeDiffHtml(const QString& diff, bool isDark) const;

};

} // namespace ui::widget::message::blocks::tools
