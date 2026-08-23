#pragma once

#include "AbstractToolBlockWidget.h"
#include "domain/agent/ToolExecution.h"

#include <functional>
#include <QList>
#include <QPair>
#include <QString>

namespace ui::widget::message::blocks {

/**
 * @brief 工具卡片工厂与注册中心（支持基于工具名动态分发和插件式扩展）
 */
class ToolWidgetFactory {
public:
    using ToolCreator = std::function<AbstractToolBlockWidget*(const domain::agent::ToolCall& call, QWidget* parent)>;

    /// 注册工具渲染器（pattern 为匹配关键词或正则前缀）
    static void registerRenderer(const QString& pattern, ToolCreator creator);

    /// 创建对应的专用工具卡片组件（若无匹配则使用 GenericToolWidget 兜底）
    static AbstractToolBlockWidget* create(const domain::agent::ToolCall& call, QWidget* parent = nullptr);

private:
    static void ensureDefaultRenderers();
    static QList<QPair<QString, ToolCreator>>& registry();
};

} // namespace ui::widget::message::blocks
