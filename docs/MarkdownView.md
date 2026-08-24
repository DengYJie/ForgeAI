# MarkdownView

`ui::widget::MarkdownView` 是 ForgeAI 的自绘 Markdown 阅读组件。它继承
`QAbstractScrollArea`，不使用 `QTextDocument` 或 HTML 作为渲染后端。

## 数据流

```text
Markdown source
  -> cmark-gfm + GFM extensions
  -> MarkdownDocument (C++ AST adapter)
  -> MarkdownLayoutEngine (QTextLayout geometry)
  -> MarkdownRenderer (QPainter)
  -> MarkdownView (scrolling and interaction)
```

`MarkdownDocument` 是 cmark-gfm 的唯一使用者；布局、渲染和控件不包含
cmark C API。布局代码不依赖 QWidget，渲染器只接收几何和主题，控件只负责
输入、滚动、事件分派和资源请求。

## 当前能力

- CommonMark 和 GFM：标题、段落、强调、删除线、链接、引用、列表、任务列表、
  代码块、表格、分隔线和独立图片。
- `QTextLayout` 处理 CJK、Emoji、RTL 和换行；`QPainter` 自绘 block 表面。
- 浅色/深色主题、运行时主题/字体/边距切换。
- 链接、文本选择、复制、代码复制反馈、上下文菜单和可选任务列表交互。
- 本地、qrc、HTTP(S) 图片缓存；图片完成后仅重绘。
- 稳定段 + 活动尾部的流式 Markdown；稳定段不在每个 chunk 重新布局。
- 以二分定位可见 block 的虚拟化绘制。

## 性能边界

一个 MarkdownView 对长文档采用 block culling：滚动不重新解析或整篇布局，绘制量
接近视口内容。它不虚拟化**多个 QWidget**；数百个 `MarkdownView` 同时构造仍会产生
相应的解析、布局和 QWidget 成本。聊天记录达到数百条时，应由 `MessageListView`
只实例化视口附近的消息卡片。

## 测试

`MarkdownCoreTests` 覆盖 parser、布局、GFM、图片资源、代码高亮、选择命中、
流式稳定段、任务列表、控件 API 和 10,000 block 虚拟化绘制。

```powershell
cmake --build build/debug --target MarkdownCoreTests -j30
ctest --test-dir build/debug --output-on-failure
```

Windows 测试目标会部署 Qt runtime 以及 `qoffscreend.dll`，因此可以用
`QT_QPA_PLATFORM=offscreen` 运行无窗口 `QApplication` 测试。

## 扩展点

- `syntax/MarkdownSyntaxHighlighter`：当前内建回退高亮；可替换为
  KSyntaxHighlighting adapter，输出仍为 `QTextLayout::FormatRange`。
- `resource/MarkdownImageResourceManager`：可扩展磁盘缓存、最大尺寸、取消和错误状态。
- `MarkdownLayoutEngine`：可继续增加复杂表格列宽、图片自然尺寸和增量 block relayout。
