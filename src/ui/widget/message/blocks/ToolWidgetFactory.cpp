#include "ToolWidgetFactory.h"
#include "tools/BashToolWidget.h"
#include "tools/FileDiffToolWidget.h"
#include "tools/FileReadToolWidget.h"
#include "tools/GenericToolWidget.h"

namespace ui::widget::message::blocks {

QList<QPair<QString, ToolWidgetFactory::ToolCreator>>& ToolWidgetFactory::registry()
{
    static QList<QPair<QString, ToolCreator>> s_registry;
    return s_registry;
}

void ToolWidgetFactory::ensureDefaultRenderers()
{
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    // 1. 终端命令与脚本执行卡片
    registerRenderer(QStringLiteral("bash"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::BashToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("run_command"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::BashToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("cmd"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::BashToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("terminal"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::BashToolWidget(call, parent);
    });

    // 2. 文件读取卡片
    registerRenderer(QStringLiteral("read_file"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileReadToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("view_file"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileReadToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("cat"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileReadToolWidget(call, parent);
    });

    // 3. 文件修改与 Diff 卡片
    registerRenderer(QStringLiteral("edit_file"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileDiffToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("replace"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileDiffToolWidget(call, parent);
    });
    registerRenderer(QStringLiteral("patch"), [](const domain::agent::ToolCall& call, QWidget* parent) {
        return new tools::FileDiffToolWidget(call, parent);
    });
}

void ToolWidgetFactory::registerRenderer(const QString& pattern, ToolCreator creator)
{
    registry().append(qMakePair(pattern.toLower(), creator));
}

AbstractToolBlockWidget* ToolWidgetFactory::create(const domain::agent::ToolCall& call, QWidget* parent)
{
    ensureDefaultRenderers();

    const QString lowerName = call.name.toLower();
    for (const auto& entry : registry()) {
        if (lowerName.contains(entry.first)) {
            return entry.second(call, parent);
        }
    }

    // 默认回退到通用卡片
    return new tools::GenericToolWidget(call, parent);
}

} // namespace ui::widget::message::blocks
