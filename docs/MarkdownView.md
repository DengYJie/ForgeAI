# MarkdownView

`ui::widget::MarkdownView` 是 ForgeAI 的自绘 Markdown 阅读组件。它继承
`QAbstractScrollArea`，不使用 `QTextDocument` 或 HTML 作为渲染后端。

## 数据流

```text
Markdown source
  -> MarkdownSourceBuffer + ParseProjection
  -> cmark-gfm full parse + ParsedDocumentSnapshot
  -> BlockReconciler + DocumentSnapshot
  -> bounded BlockLocalLayout cache + document placement
  -> LayoutSnapshot / MarkdownRenderer (QPainter)
  -> MarkdownView (scrolling and interaction)
```

`MarkdownDocument` 是 cmark-gfm 的唯一使用者。Source 使用 QString UTF-16
半开区间；parser bridge 把 cmark 的 UTF-8 source position 映射回真实 Source。
AST 每次全文生成，但 Reconciler 为未变化的顶级语义块保留 BlockId，布局缓存
只重建发生变化的块。Renderer 只读取不可变布局结果，不读取 streaming 状态。

## 当前能力

- CommonMark 和 GFM：标题、段落、强调、删除线、链接、引用、列表、任务列表、
  代码块、表格、分隔线和独立图片。
- `QTextLayout` 处理 CJK、Emoji、RTL 和换行；`QPainter` 自绘 block 表面。
- 浅色/深色主题、运行时主题/字体/边距切换。
- 链接、文本选择、复制、代码复制反馈、上下文菜单和可选任务列表交互。
- 本地、qrc、HTTP(S) 图片缓存；图片完成后仅重绘。
- 流式 chunk 以 33ms 合并；全文语义解析后仅变化块重新布局。
- List/BlockQuote 作为语义缓存单元，并展开为按 Y 排序的绘制片段进行可视域裁剪。
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
- `MarkdownLayoutEngine`：可继续增加复杂表格列宽和更细粒度的资源成本统计。
- `ParseProjection`：当前是 identity mapping，后续 StreamingNormalizer 在此层加入 synthetic tail。
