# MessageListView

`MessageListView` 是基于 Qt Widgets 的虚拟聊天消息列表。它使用
`fluent::scrolling::ScrollView` 作为滚动容器，但不使用 `QVBoxLayout`
持有所有消息卡片。

## 虚拟化模型

每条消息只保留以下轻量数据：消息本身、测量/估算高度和文档 Y 坐标。

```text
QList<Message>
  -> Item { message, y, height }
  -> visible range (binary search)
  -> MessageCardWidget viewport window + object pool
```

- 视口上下保留两个视口高度的 preload 区域。
- 滚动时，离开 preload 的卡片隐藏并放入对象池。
- 新进入区域的消息优先复用卡片；复用前调用 `resetForReuse()` 清理
  `ProcessGroupWidget`、思考块、工具块和错误块。
- 真实高度到达后刷新后续 item 的几何；用户不在底部且变化位于视口上方时，
  滚动位置会补偿高度差，避免阅读位置跳动。

## 保留的行为

- `syncMessages` 仍然是 UDF 的唯一数据入口，`ChatPage::render` 无需改变。
- `scrollToBottom`、`scrollToMessage`、自定义滚动条、顶部消息锚点和自动跟底保留。
- 角色对应的用户右侧气泡、助手左侧内容流、header/avatar 显隐、
  `ProcessGroupWidget` 和流式 `MessageCardWidget::syncMessage` 均继续由卡片负责。

## 性能验证

Qt Test 会注入 500 条用户/助手混合消息并断言：

- 实际活动卡片远少于总消息数；
- 滚到末尾后，活动卡片与池中卡片总数仍受视口窗口限制；
- header/avatar 显隐契约仍对虚拟卡片生效。

Cherry Studio 与 Codex 的消息列表仅作为阅读流、角色层级和滚底行为的视觉
参考；虚拟化、对象所有权、几何更新和事件调度均使用 Qt Widgets 原生机制实现。
