#include <QtTest>

#include "ui/markdown/MarkdownDocument.h"
#include "ui/markdown/MarkdownLayout.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/markdown/MarkdownTheme.h"
#include "ui/markdown/resource/MarkdownImageResourceManager.h"
#include "ui/markdown/syntax/MarkdownSyntaxHighlighter.h"
#include "ui/widget/MarkdownView.h"
#include "ui/widget/message/MessageListView.h"
#include "ui/widget/message/MessageCardWidget.h"
#include "ui/widget/message/ProcessGroupWidget.h"

#include <QImage>
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QPainter>
#include <QTemporaryDir>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <algorithm>
#include <memory>
#include <vector>

using namespace ui::markdown;

namespace {

QString generateLongMarkdownDocument() {
    return QStringLiteral(
        "# 1. 标题与大纲层级 (Heading Hierarchy)\n\n"
        "# Heading 1 一级标题\n"
        "## Heading 2 二级标题\n"
        "### Heading 3 三级标题\n"
        "#### Heading 4 四级标题\n"
        "##### Heading 5 五级标题\n"
        "###### Heading 6 六级标题\n\n"
        "Setext Heading 1 一级底线标题\n"
        "=============================\n\n"
        "Setext Heading 2 二级底线标题\n"
        "-----------------------------\n\n"
        "---\n\n"
        "# 2. 行内文本样式与字符格式化 (Inline Formatting)\n\n"
        "这是长文本排版测试的核心正文段落。我们支持大量中英文混排、标点符号、数学运算符以及特殊字符格式。\n\n"
        "在实际的大语言模型（LLM）对话交互中，模型经常生成包含多种行内标签的内容：\n\n"
        "- **粗体强调** (`**bold**`) 与 __粗体下划线__ (`__bold__`)\n"
        "- *斜体强调* (`*italic*`) 与 _斜体下划线_ (`_italic_`)\n"
        "- ***粗斜体混合强调*** (`***bold italic***`)\n"
        "- ~~删除线语法 (GFM Extension)~~ (`~~strikethrough~~`)\n"
        "- 内联代码：`QFile`、`QTextStream`、`QSettings` 与 `` `含反引号的转义代码` ``\n"
        "- 交互式超链接：[ForgeAI 官方仓库 (Hover测试)](https://github.com/DengYJie/ForgeAI)\n"
        "- GFM 自动链接识别：https://github.com 与 <support@forge.ai>\n"
        "- 行内 HTML 标签：<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>P</kbd> 以及 <span>自定义行内文字</span>\n"
        "- 强制换行测试：\n"
        "  第一行文本（末尾带反斜杠）\\\n"
        "  强制折行到第二行展示。\n\n"
        "---\n\n"
        "# 3. 多层引用块与格式嵌套 (Blockquotes)\n\n"
        "> 单层引用块：ForgeAI 采用基于 QTextLayout 的分层排版引擎，将文本准备与几何排版彻底解耦。\n"
        ">\n"
        "> > 嵌套第二层引用：解耦了 Preferred Size 测算与 Actual Viewport Layout 实际排版。\n"
        "> >\n"
        "> > > 嵌套第三层引用：支持极速流式增量追加，仅重新排版 tailDocument，绝不破坏 stableDocument 缓存。\n\n"
        "> 带内部复杂格式的引用块：\n"
        "> - 引用内的列表项 1：`inline code` 高亮显示\n"
        "> - 引用内的列表项 2：**加粗的架构结论**，确保行间距均匀\n"
        "> - 引用内的表格测试：\n"
        ">   | 属性 | 描述 |\n"
        ">   |---|---|\n"
        ">   | Intrinsic | 宽度不变的内在尺寸 |\n"
        ">   | Wrapping | 随宽度压缩的折行排版 |\n\n"
        "---\n\n"
        "# 4. 复杂列表与任务清单 (Lists & Task Lists)\n\n"
        "### 4.1 无序列表与三层嵌套\n"
        "- 顶级项目 A (`-` 标记)\n"
        "  * 次级嵌套项目 A.1 (`*` 标记)\n"
        "    + 三级嵌套项目 A.1.1 (`+` 标记)\n"
        "    + 三级嵌套项目 A.1.2\n"
        "  * 次级嵌套项目 A.2\n"
        "- 顶级项目 B\n\n"
        "### 4.2 有序步骤清单\n"
        "1. 第一步：解析 Markdown 抽象语法树（基于 cmark-gfm 高性能 C 解析库）\n"
        "2. 第二步：执行分块几何布局（MarkdownLayoutEngine）\n"
        "   1. 计算行高与字形折行（HarfBuzz shaping 仅在宽度压缩时发生）\n"
        "   2. 缓存测量尺寸（LRU Width-Aware 缓存机制）\n"
        "3. 第三步：视口裁剪并高效绘制（MarkdownRenderer 仅遍历可见 BlockLayout）\n\n"
        "### 4.3 GFM 任务清单 (带交互复选框状态)\n"
        "- [x] 已完成的核心特性：CommonMark 基础语法解析与 AST 节点构建\n"
        "- [x] 已完成的核心特性：GFM 表格与删除线扩展支持\n"
        "- [x] 已完成的核心特性：内联代码块圆角胶囊与选中背景绘制\n"
        "- [x] 已完成的性能优化：CodeBlock 语法高亮与 QTextLayout 宽度不变缓存\n"
        "- [x] 已完成的性能优化：Table Intrinsic 自然宽度测算与列宽复用\n"
        "- [x] 已完成的性能优化：Streaming 细粒度缓存保留与局部尾部增量排版\n"
        "- [ ] 待优化的扩展项：16ms 窗口缩放事件帧合并调度\n"
        "- [ ] 待优化的扩展项：Interactive Resize 预加载视口降低\n\n"
        "---\n\n"
        "# 5. 多列 GFM 数据表格 (Multi-Column Tables)\n\n"
        "| 模块特性 | 默认对齐 | 居中对齐 (`:---:`) | 靠右对齐 (`---:`) | 性能表现 | 状态 |\n"
        "|:---|:---|:---:|---:|:---:|:---:|\n"
        "| **基础解析** | cmark-gfm AST | AST 节点遍历 | 0.8 ms | 极速 | ✅ 已就绪 |\n"
        "| **行内样式** | `*italic*`, `**bold**` | `~~strike~~` | 1.2 ms | 极速 | ✅ 已就绪 |\n"
        "| **代码高亮** | C++, Python, JSON | 关键词/字符串/注释 | 2.5 ms | 缓存复用 | ✅ 已就绪 |\n"
        "| **视口裁剪** | 仅绘制可见 Block | `firstVisibleBlock` | 0.3 ms | 极致 | ✅ 已就绪 |\n"
        "| **流式排版** | 稳定块锁定 + 尾部排版 | 增量 Append | < 1.0 ms | 极速 | ✅ 已就绪 |\n"
        "| **窗口缩放** | 实时测量 | 帧合并 + LRU 缓存 | 2.1 ms | 丝滑 | 🚀 稳定交付 |\n\n"
        "---\n\n"
        "# 6. 语法高亮代码块 (Fenced Code Blocks)\n\n"
        "### 6.1 C++ 核心代码示例\n"
        "```cpp\n"
        "#include <iostream>\n"
        "#include <memory>\n"
        "#include \"ui/markdown/MarkdownLayout.h\"\n\n"
        "// 计算窗口缩放时的视口几何与局部排版\n"
        "int calculateOptimalHeight(int availableWidth, const ui::markdown::MarkdownTheme& theme) {\n"
        "    const double ratio = 16.0 / 9.0;\n"
        "    int calculatedHeight = static_cast<int>(availableWidth / ratio) + 24;\n"
        "    std::cout << \"Calculated: \" << calculatedHeight << std::endl;\n"
        "    return calculatedHeight;\n"
        "}\n\n"
        "void processLayoutPipeline(const ui::markdown::DocumentLayout& doc) {\n"
        "    for (const auto& block : doc.blocks) {\n"
        "        if (block.kind == ui::markdown::BlockKind::CodeBlock) {\n"
        "            std::cout << \"Code block lines: \" << block.codeLines.size() << std::endl;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "```\n\n"
        "### 6.2 Python 异步流水线示例\n"
        "```python\n"
        "import asyncio\n"
        "import time\n"
        "from typing import AsyncGenerator\n\n"
        "async def token_stream_simulator(content: str, chunk_size: int = 12) -> AsyncGenerator[str, None]:\n"
        "    \"\"\"模拟 LLM 真实 Token 流式投递流水线\"\"\"\n"
        "    for i in range(0, len(content), chunk_size):\n"
        "        chunk = content[i:i + chunk_size]\n"
        "        await asyncio.sleep(0.015)  # 15ms 模拟 Token 生成间隔\n"
        "        yield chunk\n\n"
        "async def main():\n"
        "    async for token in token_stream_simulator(\"Hello ForgeAI Streaming!\"):\n"
        "        print(token, end=\"\", flush=True)\n"
        "```\n\n"
        "### 6.3 Rust 高性能流处理示例\n"
        "```rust\n"
        "use std::time::Instant;\n\n"
        "pub struct LayoutBenchmark {\n"
        "    pub total_passes: u64,\n"
        "    pub avg_latency_ms: f64,\n"
        "}\n\n"
        "impl LayoutBenchmark {\n"
        "    pub fn run_benchmark(&mut self) -> bool {\n"
        "        let start = Instant::now();\n"
        "        // 模拟 10,000 次排版计算\n"
        "        let elapsed = start.elapsed();\n"
        "        self.avg_latency_ms = elapsed.as_secs_f64() * 1000.0 / 10000.0;\n"
        "        self.avg_latency_ms < 0.05\n"
        "    }\n"
        "}\n"
        "```\n\n"
        "### 6.4 JSON 配置数据\n"
        "```json\n"
        "{\n"
        "  \"projectName\": \"ForgeAI\",\n"
        "  \"version\": \"1.8.0\",\n"
        "  \"optimization\": {\n"
        "    \"streamingTailOnly\": true,\n"
        "    \"widthAwareLRUCache\": true,\n"
        "    \"tableIntrinsicFastPath\": true,\n"
        "    \"codeBlockHighlightCache\": true\n"
        "  },\n"
        "  \"benchmarkTargetFps\": 60\n"
        "}\n"
        "```\n\n"
        "---\n\n"
        "# 7. 嵌入式组件与富媒体展示 (Images & Embeds)\n\n"
        "![ForgeAI 系统架构设计图 (测试自适应尺寸)](architecture_diagram_preview.png)\n\n"
        "---\n\n"
        "# 8. 原始 HTML 自定义组件块 (Raw HTML Blocks)\n\n"
        "<div style=\"border: 1px solid #0f6cbd; padding: 12px; border-radius: 8px; background: rgba(15, 108, 189, 0.05);\">\n"
        "    <b>长文本流式压力测试提示：</b> 本文档综合覆盖了各种复杂语法节点，是检测流式排版、内存局部性与窗口缩放稳定性的基准样本。\n"
        "</div>\n\n"
        "---\n\n"
        "# 9. 分割线样式测试 (Thematic Breaks)\n\n"
        "分割线形式 A：\n"
        "---\n"
        "分割线形式 B：\n"
        "***\n"
        "分割线形式 C：\n"
        "___\n\n"
        "# 10. 总结与展望 (Conclusion)\n\n"
        "至此，长文档的全部 10 个章节已全部流式输出完成。排版引擎在整个流式输出过程中保持了极高的吞吐率和极低的帧延迟。"
    );
}

} // namespace

class MarkdownCoreTests final : public QObject {
    Q_OBJECT
private slots:
    void parsesCommonMarkAndGfm();
    void handlesEmptyAndIncompleteInput();
    void layoutIsStableAcrossWidths();
    void laysOutNestedListsWithoutOverlap();
    void paintsVisibleBlocks();
    void markdownViewSupportsDisplaySelectionAndCopy();
    void loadsLocalImageResources();
    void highlightsCommonCodeLanguages();
    void codeBlocksProvideTextHitTesting();
    void streamingFinishesWithCompleteDocumentLayout();
    void streamingKeepsStableBlocksOutOfTailRelayout();
    void streamingLongDocumentPerformanceAndStability();
    void markdownViewThemeAndSelectionApi();
    void markdownViewFollowsFluentDarkTheme();
    void taskListsHaveStableHitTargetsAndOptionalInteraction();
    void virtualizesLargeDocumentPainting();
    void laysOutUnicodeAndHighDpiContent();
    void reportsManyViewConstructionCost();
    void virtualizesMessageCards();
    void virtualizesThousandsOfMessageRecords();
    void userCardHeightForWidthCalculationIsAccurate();
    void fitContentSizeHintIsInvariantUnderActualResizes();
    void heightForWidthRespondsToWrappingWidth();
    void autoFitHeightContractIsRespected();
    void inlineCodeSelectionPaintsSelectionBackground();
    void headingThemeColorIsApplied();
    void linkHoverHighlightIsApplied();
    void coldLayoutCachesWidthInvariantsAndFastPaths();
    
    // P0/P1 Regression Tests
    void streamingIncrementalTextIsAlwaysCurrent();
    void streamingTailTaskCheckRectIsInsideBlockRect();
    void streamingTailCodeScrollInfoIsConsistentWithBlockRect();
    void streamingStableImageHeightDoesNotRegressOnNextToken();
    void streamingFinishGeometryMatchesFinalLayout();
    void streamingFinishedEmittedAfterFinalLayout();
    void fencedCodeNotClosedByDifferentMarker();
    
    void visualTest();
};

void MarkdownCoreTests::parsesCommonMarkAndGfm()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral(
        "# 标题 😀\n\nHello **strong** and ~~removed~~.\n\n- [x] done\n\n| A | B |\n|---|---|\n| 1 | 2 |\n"));
    QCOMPARE(document.root().type, MarkdownNodeType::Document);
    QVERIFY(document.root().children.size() >= 4);
    QCOMPARE(document.root().children[0]->type, MarkdownNodeType::Heading);
    QCOMPARE(document.root().children[2]->type, MarkdownNodeType::List);
    QVERIFY(document.root().children[2]->children[0]->attributes.taskListItem);
    QVERIFY(document.root().children[2]->children[0]->attributes.taskChecked);
    QCOMPARE(document.root().children[3]->type, MarkdownNodeType::Table);
}

void MarkdownCoreTests::handlesEmptyAndIncompleteInput()
{
    MarkdownParser parser;
    QVERIFY(parser.parse({}).isEmpty());
    const MarkdownDocument partial = parser.parse(QStringLiteral("**unclosed `code\n```cpp\nint value = 1;"));
    QVERIFY(!partial.isEmpty());
}

void MarkdownCoreTests::layoutIsStableAcrossWidths()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral(
        "# Heading\n\n普通文本 + **bold** + `inline code` + [link](https://example.com) + emoji 😀\n\n> quote\n\n```cpp\nint main() { return 0; }\n```"));
    MarkdownLayoutEngine engine;
    const MarkdownTheme theme = MarkdownTheme::light();
    const DocumentLayout narrow = engine.layout(document, 300, theme);
    const DocumentLayout medium = engine.layout(document, 600, theme);
    const DocumentLayout wide = engine.layout(document, 1000, theme);
    QVERIFY(!narrow.blocks.isEmpty());
    QVERIFY(narrow.size.height() > 0);
    QVERIFY(medium.size.height() > 0);
    QVERIFY(wide.size.height() > 0);
    QVERIFY(narrow.size.height() >= wide.size.height());
    QCOMPARE(engine.layout(document, 600, theme).size, medium.size);
}

void MarkdownCoreTests::laysOutNestedListsWithoutOverlap()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral("1. parent\n   - child one\n   - child two\n2. final"));
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 360, MarkdownTheme::light());
    QVERIFY(layout.blocks.size() >= 4);
    for (qsizetype i = 1; i < layout.blocks.size(); ++i)
        QVERIFY(layout.blocks[i - 1].rect.bottom() <= layout.blocks[i].rect.top());
}

void MarkdownCoreTests::paintsVisibleBlocks()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral(
        "# Render fixture\n\n> quote\n\n- [ ] pending\n- [x] done\n\n| one | two |\n|---|---|\n| A | B |\n\n```cpp\nint answer = 42;\n```"));
    const MarkdownTheme theme = MarkdownTheme::light();
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 480, theme);
    constexpr qreal previewDpr = 4.0;
    QImage image(qCeil(480 * previewDpr), qCeil(layout.size.height() * previewDpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(previewDpr);
    image.fill(Qt::white);
    QPainter painter(&image);
    MarkdownRenderer{}.paint(painter, layout, theme, QRectF(0, 0, 480, layout.size.height()), {});
    painter.end();
    const QString previewPath = qEnvironmentVariable("MARKDOWN_RENDER_PREVIEW");
    if (!previewPath.isEmpty()) QVERIFY2(image.save(previewPath), qPrintable(previewPath));
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::white));
    bool changed = false;
    for (int y = 0; y < image.height() && !changed; ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) != QColor(Qt::white)) { changed = true; break; }
    QVERIFY(changed);
}

void MarkdownCoreTests::markdownViewSupportsDisplaySelectionAndCopy()
{
    ui::widget::MarkdownView view;
    view.resize(480, 240);
    view.setAutoFitHeight(false);
    view.setMarkdown(QStringLiteral("# 标题\n\nHello **Qt** with [link](https://example.com).\n\n```cpp\nint value = 42;\n```"));
    view.show();
    QCoreApplication::processEvents();
    view.setFocus();
    QCoreApplication::processEvents();
    QVERIFY(view.hasFocus());
    QVERIFY(view.sizeHint().height() > 24);
    QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
    QVERIFY(!view.selectedText().isEmpty());
    view.copy();
    QCOMPARE(QApplication::clipboard()->text(), view.selectedText());
}

void MarkdownCoreTests::loadsLocalImageResources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("sample.png"));
    QImage expected(6, 4, QImage::Format_ARGB32_Premultiplied);
    expected.fill(QColor("#cc3355"));
    QVERIFY(expected.save(imagePath));

    MarkdownImageResourceManager resources;
    const QString source = QUrl::fromLocalFile(imagePath).toString();
    resources.request(source, {});
    QVERIFY(resources.images().contains(source));
    QCOMPARE(resources.images().value(source).pixelColor(0, 0), QColor("#cc3355"));

    const MarkdownDocument document = MarkdownParser{}.parse(QStringLiteral("![sample](%1)").arg(source));
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 400, MarkdownTheme::light());
    QVERIFY(!layout.blocks.isEmpty());
    QCOMPARE(layout.blocks.first().kind, BlockKind::Image);
}

void MarkdownCoreTests::highlightsCommonCodeLanguages()
{
    const MarkdownTheme theme = MarkdownTheme::dark();
    MarkdownSyntaxHighlighter highlighter;
    const auto cppFormats = highlighter.highlightLine(QStringLiteral("int answer = 42; // comment"), QStringLiteral("cpp"), theme);
    const auto pythonFormats = highlighter.highlightLine(QStringLiteral("def greet(name): return 'hello'"), QStringLiteral("python"), theme);
    QVERIFY(cppFormats.size() >= 3);
    QVERIFY(pythonFormats.size() >= 2);
    QVERIFY(highlighter.highlightLine(QStringLiteral("just words"), QStringLiteral("unknown"), theme).isEmpty());
}

void MarkdownCoreTests::codeBlocksProvideTextHitTesting()
{
    const MarkdownDocument document = MarkdownParser{}.parse(QStringLiteral("Before\n\n```cpp\nint first = 1;\nint second = 2;\n```\n\nAfter"));
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 480, MarkdownTheme::light());
    int codeIndex = -1;
    for (int i = 0; i < layout.blocks.size(); ++i)
        if (layout.blocks.at(i).kind == BlockKind::CodeBlock) { codeIndex = i; break; }
    QVERIFY(codeIndex >= 0);
    const BlockLayout& code = layout.blocks.at(codeIndex);
    const MarkdownRenderer renderer;
    const auto first = renderer.hitTest(layout, QPointF(code.contentX + 4, code.rect.top() + 36));
    const auto second = renderer.hitTest(layout, QPointF(code.contentX + 4, code.rect.top() + 36 + code.codeLines.at(0)->height));
    QCOMPARE(first.kind, HitKind::Text);
    QCOMPARE(second.kind, HitKind::Text);
    QVERIFY(second.textOffset > first.textOffset);
}

void MarkdownCoreTests::streamingFinishesWithCompleteDocumentLayout()
{
    const QString markdown = QStringLiteral("Stable paragraph.\n\n```cpp\nint answer = 42;\n```\n\nTail **content**.");
    ui::widget::MarkdownView streamed;
    streamed.resize(460, 260);
    streamed.beginStream();
    streamed.appendMarkdown(QStringLiteral("Stable paragraph.\n\n```cpp\nint ans"));
    streamed.appendMarkdown(QStringLiteral("wer = 42;\n```\n\nTail **content**."));
    streamed.finishStream();
    QCOMPARE(streamed.markdown(), markdown);

    ui::widget::MarkdownView complete;
    complete.resize(460, 260);
    complete.setMarkdown(markdown);
    QCOMPARE(streamed.sizeHint().height(), complete.sizeHint().height());
}

void MarkdownCoreTests::streamingKeepsStableBlocksOutOfTailRelayout()
{
    ui::widget::MarkdownView view;
    view.resize(480, 260);
    view.beginStream();
    view.appendStreamingText(QStringLiteral("Stable block.\n\n"));
    const ui::widget::MarkdownViewMetrics afterStable = view.metrics();
    QVERIFY(afterStable.stableLayoutCount > 0);

    for (int i = 0; i < 120; ++i)
        view.appendStreamingText(QStringLiteral("tail%1 ").arg(i));
    const ui::widget::MarkdownViewMetrics duringTail = view.metrics();
    QCOMPARE(duringTail.stableLayoutCount, afterStable.stableLayoutCount);
    QCOMPARE(duringTail.stableParseCount, afterStable.stableParseCount);
    QVERIFY(duringTail.tailLayoutCount >= afterStable.tailLayoutCount + 120);
    QVERIFY(duringTail.tailParseCount >= afterStable.tailParseCount + 120);

    view.appendStreamingText(QStringLiteral("\n\nnext stable block.\n\n"));
    QVERIFY(view.metrics().stableLayoutCount > afterStable.stableLayoutCount);
    view.finishStreaming();
}

void MarkdownCoreTests::streamingLongDocumentPerformanceAndStability()
{
    const QString longDoc = generateLongMarkdownDocument();
    QVERIFY(longDoc.length() >= 4000);

    // 1. Break into realistic LLM streaming token chunks (5-20 characters per chunk, ~300 chunks)
    QStringList chunks;
    int pos = 0;
    int step = 14;
    while (pos < longDoc.length()) {
        const int chunkSize = qMin(step, static_cast<int>(longDoc.length()) - pos);
        chunks.append(longDoc.mid(pos, chunkSize));
        pos += chunkSize;
        step = (step % 17) + 6;
    }
    QVERIFY(chunks.size() >= 150);

    // 2. Stream through MarkdownView
    ui::widget::MarkdownView streamed;
    streamed.resize(640, 480);
    streamed.beginStream();

    QElapsedTimer streamTimer;
    streamTimer.start();
    qint64 totalTailUs = 0;
    qint64 maxTailUs = 0;

    for (const QString& chunk : chunks) {
        QElapsedTimer tokenTimer;
        tokenTimer.start();
        streamed.appendStreamingText(chunk);
        const qint64 tokenUs = tokenTimer.nsecsElapsed() / 1000;
        totalTailUs += tokenUs;
        maxTailUs = qMax(maxTailUs, tokenUs);
    }
    const qint64 totalMs = streamTimer.elapsed();
    streamed.finishStreaming();

    // 3. Verify metrics & invariants
    const auto m = streamed.metrics();
    QCOMPARE(streamed.markdown(), longDoc);
    QVERIFY(m.tailLayoutCount >= static_cast<quint64>(chunks.size()));
    // Stable layout should only trigger when complete blocks form, strictly bounded << chunks.size()
    QVERIFY(m.stableLayoutCount < static_cast<quint64>(chunks.size() / 2));

    const double avgTailMs = (totalTailUs / 1000.0) / chunks.size();
    qInfo() << "Long document streaming benchmark: total chunks =" << chunks.size()
            << ", total streaming time =" << totalMs << "ms"
            << ", avg per-token latency =" << avgTailMs << "ms"
            << ", max token latency =" << (maxTailUs / 1000.0) << "ms"
            << ", stableLayoutCount =" << m.stableLayoutCount
            << ", tailLayoutCount =" << m.tailLayoutCount;

    QVERIFY2(avgTailMs < 5.0, qPrintable(QStringLiteral("Average per-token latency too slow: %1 ms").arg(avgTailMs)));

    // 4. Validate layout parity with non-streamed full parse
    ui::widget::MarkdownView batch;
    batch.resize(640, 480);
    batch.setMarkdown(longDoc);

    QCOMPARE(streamed.sizeHint().height(), batch.sizeHint().height());
    QCOMPARE(streamed.sizeHint().width(), batch.sizeHint().width());

    // 5. Verify interaction on streamed content
    streamed.selectAll();
    batch.selectAll();
    QVERIFY(!streamed.selectedText().isEmpty());
    QCOMPARE(streamed.selectedText().length(), batch.selectedText().length());
}

void MarkdownCoreTests::markdownViewThemeAndSelectionApi()
{
    ui::widget::MarkdownView view;
    view.resize(420, 220);
    view.setMarkdown(QStringLiteral("Theme **switch** test."));
    const QString source = view.markdown();
    const ui::markdown::MarkdownTheme dark = ui::markdown::MarkdownTheme::dark();
    view.setTheme(dark);
    QCOMPARE(view.markdown(), source);
    QCOMPARE(view.theme().version, dark.version);
    view.selectAll();
    QVERIFY(!view.selectedText().isEmpty());
    view.setSelectable(false);
    QVERIFY(!view.isSelectable());
    QVERIFY(view.selectedText().isEmpty());
    view.setSelectable(true);
    view.clear();
    QVERIFY(view.markdown().isEmpty());
}

void MarkdownCoreTests::markdownViewFollowsFluentDarkTheme()
{
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    ui::widget::MarkdownView view;
    view.onThemeUpdated();
    QVERIFY(view.theme().text.lightness() > 160);
    const auto document = MarkdownParser{}.parse(QStringLiteral("Plain dark body text"));
    const auto layout = MarkdownLayoutEngine{}.layout(document, 420, view.theme());
    QVERIFY(!layout.blocks.isEmpty());
    const auto formats = layout.blocks.first().inlineLayout->layout.formats();
    QVERIFY(!formats.isEmpty());
    QCOMPARE(formats.first().format.foreground().color(), view.theme().text);
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

void MarkdownCoreTests::taskListsHaveStableHitTargetsAndOptionalInteraction()
{
    const QString markdown = QStringLiteral("- [ ] todo\n- [x] done");
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(MarkdownParser{}.parse(markdown), 360, MarkdownTheme::light());
    QVERIFY(layout.blocks.size() >= 2);
    QVERIFY(layout.blocks.at(0).taskItem);
    QVERIFY(!layout.blocks.at(0).taskChecked);
    QVERIFY(layout.blocks.at(1).taskChecked);
    const auto hit = MarkdownRenderer{}.hitTest(layout, layout.blocks.at(0).taskCheckRect.center());
    QCOMPARE(hit.kind, HitKind::TaskCheckbox);

    ui::widget::MarkdownView view;
    view.resize(360, 180);
    view.setTaskListInteractive(true);
    view.setMarkdown(markdown);
    view.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, layout.blocks.at(0).taskCheckRect.center().toPoint());
    QVERIFY(view.markdown().contains(QStringLiteral("- [x] todo")));
}

void MarkdownCoreTests::virtualizesLargeDocumentPainting()
{
    QString markdown;
    markdown.reserve(220000);
    for (int i = 0; i < 10000; ++i)
        markdown += QStringLiteral("Paragraph %1: long enough content for ordinary document layout.\n\n").arg(i);
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(MarkdownParser{}.parse(markdown), 560, MarkdownTheme::light());
    QCOMPARE(layout.blocks.size(), 10000);
    QImage image(560, 320, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    const int painted = MarkdownRenderer{}.paint(painter, layout, MarkdownTheme::light(), QRectF(0, 0, 560, 320), {});
    painter.end();
    QVERIFY(painted > 0);
    QVERIFY2(painted < 100, "Painting must be proportional to viewport-visible blocks, not all 10,000 blocks.");
}

void MarkdownCoreTests::laysOutUnicodeAndHighDpiContent()
{
    const QString markdown = QStringLiteral("中文 日本語 한국어 — العربية עברית 😀 👩‍💻 e\u0301\n\n**粗体** and `code`.");
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(MarkdownParser{}.parse(markdown), 360, MarkdownTheme::light());
    QVERIFY(!layout.blocks.isEmpty());
    QVERIFY(layout.size.height() > 0);
    QImage image(720, 480, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(Qt::white);
    QPainter painter(&image);
    const int painted = MarkdownRenderer{}.paint(painter, layout, MarkdownTheme::light(), QRectF(0, 0, 360, 240), {});
    painter.end();
    QVERIFY(painted > 0);
}

void MarkdownCoreTests::reportsManyViewConstructionCost()
{
    constexpr int viewCount = 300;
    QElapsedTimer timer;
    timer.start();
    std::vector<std::unique_ptr<ui::widget::MarkdownView>> views;
    views.reserve(viewCount);
    for (int index = 0; index < viewCount; ++index) {
        auto view = std::make_unique<ui::widget::MarkdownView>();
        view->resize(520, 200);
        view->setAutoFitHeight(true);
        view->setMarkdown(QStringLiteral("Message %1: **Markdown** with `code`, [link](https://example.com), and 中文 😀.").arg(index));
        views.push_back(std::move(view));
    }
    const qint64 elapsedMs = timer.elapsed();
    qInfo().noquote() << QStringLiteral("MarkdownView benchmark: %1 views initialized in %2 ms.").arg(viewCount).arg(elapsedMs);
    QCOMPARE(static_cast<int>(views.size()), viewCount);
}

void MarkdownCoreTests::virtualizesMessageCards()
{
    QList<domain::conversation::Message> messages;
    for (int index = 0; index < 500; ++index) {
        domain::conversation::Message message;
        message.id = QUuid::createUuid();
        message.role = index % 2 ? domain::MessageRole::Assistant : domain::MessageRole::User;
        message.createdAt = QDateTime::currentDateTimeUtc();
        message.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("Message %1 with **markdown** content.").arg(index)}});
        if (index == 1) {
            message.blocks.append(domain::conversation::MessageBlock{
                domain::BlockType::Thought, domain::conversation::ThoughtBlock{QStringLiteral("Planning a response."), 120}});
        }
        messages.append(std::move(message));
    }
    ui::widget::message::MessageListView list;
    list.resize(620, 480);
    list.show();
    list.syncMessages(messages);
    QCoreApplication::processEvents();
    const QString previewPath = qEnvironmentVariable("MESSAGE_LIST_PREVIEW");
    if (!previewPath.isEmpty()) QVERIFY2(list.viewport()->grab().save(previewPath), qPrintable(previewPath));
    list.setAvatarVisible(false);
    list.setHeaderVisible(false);
    QCoreApplication::processEvents();
    QCOMPARE(list.messageCount(), 500);
    QVERIFY(list.activeCardCount() > 0);
    QVERIFY2(list.activeCardCount() < 80, "Only the viewport preload window may own card widgets.");
    const auto cards = list.findChildren<ui::widget::message::MessageCardWidget*>();
    QVERIFY(!cards.isEmpty());
    for (const auto* card : cards) {
        QVERIFY(!card->isAvatarVisible());
        QVERIFY(!card->isHeaderVisible());
    }
    bool hasVisibleCard = false;
    for (const auto* card : cards) {
        if (card->isVisible() && card->geometry().width() > 0 && card->geometry().height() > 0) {
            hasVisibleCard = true;
            QVERIFY2(card->height() < 220, "A short message must not retain a narrow-width measurement height.");
            break;
        }
    }
    QVERIFY(hasVisibleCard);
    QVERIFY(!list.findChildren<ui::widget::message::ProcessGroupWidget*>().isEmpty());

    QSignalSpy topVisibleSpy(&list, &ui::widget::message::MessageListView::topVisibleMessageChanged);
    list.scrollToMessage(messages.at(250).id);
    QTest::qWait(40);
    QVERIFY(list.verticalScrollBar()->value() > 0);
    QVERIFY(!topVisibleSpy.isEmpty());

    QScrollBar externalScrollBar(&list);
    list.setCustomScrollBar(&externalScrollBar);
    QCOMPARE(externalScrollBar.maximum(), list.verticalScrollBar()->maximum());
    list.verticalScrollBar()->setValue(list.verticalScrollBar()->maximum());
    QCoreApplication::processEvents();
    QVERIFY(list.activeCardCount() < 80);
    QVERIFY2(list.activeCardCount() + list.pooledCardCount() < 100,
             "Scrolling must recycle cards instead of allocating one widget per message.");
}

void MarkdownCoreTests::virtualizesThousandsOfMessageRecords()
{
    QList<domain::conversation::Message> messages;
    messages.reserve(2000);
    for (int index = 0; index < 2000; ++index) {
        domain::conversation::Message message;
        message.id = QUuid::createUuid();
        message.role = index % 2 ? domain::MessageRole::Assistant : domain::MessageRole::User;
        message.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text, domain::conversation::TextBlock{QStringLiteral("Virtual record %1.").arg(index)}});
        messages.append(std::move(message));
    }
    ui::widget::message::MessageListView list;
    list.resize(640, 500);
    list.show();
    QElapsedTimer timer;
    timer.start();
    list.syncMessages(messages);
    QCoreApplication::processEvents();
    qInfo() << "MessageListView benchmark: 2000 records synchronized in" << timer.elapsed() << "ms; active cards:" << list.activeCardCount();
    QCOMPARE(list.messageCount(), 2000);
    QVERIFY(list.activeCardCount() > 0);
    QVERIFY(list.activeCardCount() < 100);
}

void MarkdownCoreTests::userCardHeightForWidthCalculationIsAccurate()
{
    domain::conversation::Message userMsg;
    userMsg.id = QUuid::createUuid();
    userMsg.role = domain::MessageRole::User;
    userMsg.blocks.append(domain::conversation::MessageBlock{
        domain::BlockType::Text,
        domain::conversation::TextBlock{QStringLiteral("### 提问 #151\n在开发 Qt 应用程序时，`QWidget::resizeEvent` 高频触发有哪些优化手段？")}
    });

    ui::widget::message::MessageCardWidget card;
    card.setMessage(userMsg);
    card.setAvailableWidth(800);
    QCoreApplication::processEvents();

    const int h800 = card.heightForWidth(800);
    // User card with Heading + Paragraph must NOT be squashed to 24px fallback!
    QVERIFY2(h800 >= 60, qPrintable(QStringLiteral("User card height (%1 px) is too small, likely squashed to fallback!").arg(h800)));
    QVERIFY(card.sizeHint().height() >= 60);

    const int h300 = card.heightForWidth(300);
    // Narrower width wraps text and requires more height
    QVERIFY(h300 >= h800);
}

void MarkdownCoreTests::fitContentSizeHintIsInvariantUnderActualResizes()
{
    ui::widget::MarkdownView view;
    view.setHorizontalSizingMode(ui::widget::MarkdownView::HorizontalSizingMode::FitContent);
    view.setPreferredWidthLimit(600);
    view.setMarkdown(QStringLiteral("如何学习qt"));
    QCoreApplication::processEvents();

    const int expectedWidth = view.sizeHint().width();
    const int expectedHeight = view.sizeHint().height();
    QVERIFY(expectedWidth > 0);
    QVERIFY(expectedHeight > 0);

    const qreal singleCharWidth = QFontMetricsF(view.theme().bodyFont).horizontalAdvance(QStringLiteral("如"));
    QVERIFY2(expectedWidth > singleCharWidth * 2,
             "Preferred width must represent the whole phrase, not collapse into a single character.");

    const QVector<QSize> testSizes = {
        QSize(20, 100),
        QSize(300, 100),
        QSize(40, 100),
        QSize(500, 100),
        QSize(10, 200),
        QSize(800, 200)
    };

    for (const auto& sz : testSizes) {
        view.resize(sz);
        QCoreApplication::processEvents();
        QCOMPARE(view.sizeHint().width(), expectedWidth);
        QCOMPARE(view.sizeHint().height(), expectedHeight);
    }
}

void MarkdownCoreTests::heightForWidthRespondsToWrappingWidth()
{
    ui::widget::MarkdownView view;
    view.setAutoFitHeight(true);
    view.setMarkdown(QStringLiteral(
        "这是一个用于测试 heightForWidth 行为的长段落，包含多行文字以确保在窄视口下会产生明显的折行。"
        "当宽度变窄时，行数增加，高度应当严格大于宽视口下的高度。"));
    QCoreApplication::processEvents();

    const int narrowHeight = view.heightForWidth(50);
    const int wideHeight = view.heightForWidth(500);

    QVERIFY(narrowHeight > wideHeight);
}

void MarkdownCoreTests::autoFitHeightContractIsRespected()
{
    ui::widget::MarkdownView view;
    view.setAutoFitHeight(false);
    view.setMarkdown(QStringLiteral("# Title\n\nParagraph 1\n\nParagraph 2\n\nParagraph 3"));
    QCoreApplication::processEvents();

    QSignalSpy heightSpy(&view, &ui::widget::MarkdownView::autoFitHeightChanged);
    view.resize(200, 100);
    QCoreApplication::processEvents();

    QCOMPARE(heightSpy.count(), 0);
}

void MarkdownCoreTests::inlineCodeSelectionPaintsSelectionBackground()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral("- `QFile`、`QTextStream`"));
    const MarkdownTheme theme = MarkdownTheme::light();
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 400, theme);
    QVERIFY(!layout.blocks.isEmpty());
    QVERIFY(layout.blocks[0].inlineLayout);
    QVERIFY(!layout.blocks[0].inlineLayout->codeSpans.isEmpty());

    QImage unselected(400, 50, QImage::Format_ARGB32_Premultiplied);
    unselected.fill(Qt::white);
    QPainter p1(&unselected);
    MarkdownRenderer{}.paint(p1, layout, theme, QRectF(0, 0, 400, 50), {});
    p1.end();

    QImage selected(400, 50, QImage::Format_ARGB32_Premultiplied);
    selected.fill(Qt::white);
    QPainter p2(&selected);
    MarkdownRenderer{}.paint(p2, layout, theme, QRectF(0, 0, 400, 50), TextSelection{0, 50});
    p2.end();

    QVERIFY(unselected != selected);
}

void MarkdownCoreTests::headingThemeColorIsApplied()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral("# Custom Heading\n\nRegular text"));
    MarkdownTheme theme = MarkdownTheme::light();
    theme.heading = QColor("#ff0000");
    theme.text = QColor("#0000ff");
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 400, theme);
    QVERIFY(layout.blocks.size() >= 2);
    QVERIFY(layout.blocks[0].inlineLayout);
    QVERIFY(layout.blocks[1].inlineLayout);

    const auto formats0 = layout.blocks[0].inlineLayout->layout.formats();
    QVERIFY(!formats0.isEmpty());
    QCOMPARE(formats0[0].format.foreground().color(), QColor("#ff0000"));

    const auto formats1 = layout.blocks[1].inlineLayout->layout.formats();
    QVERIFY(!formats1.isEmpty());
    QCOMPARE(formats1[0].format.foreground().color(), QColor("#0000ff"));
}

void MarkdownCoreTests::linkHoverHighlightIsApplied()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral("Visit [ForgeAI](https://forge.ai) for more"));
    MarkdownTheme theme = MarkdownTheme::light();
    theme.link = QColor("#0000ff");
    theme.linkHover = QColor("#ff00ff");
    const DocumentLayout layout = MarkdownLayoutEngine{}.layout(document, 400, theme);

    QImage normal(400, 40, QImage::Format_ARGB32_Premultiplied);
    normal.fill(Qt::white);
    QPainter p1(&normal);
    MarkdownRenderer{}.paint(p1, layout, theme, QRectF(0, 0, 400, 40), {}, -1, -1, {}, {}, -1, QString());
    p1.end();

    QImage hovered(400, 40, QImage::Format_ARGB32_Premultiplied);
    hovered.fill(Qt::white);
    QPainter p2(&hovered);
    MarkdownRenderer{}.paint(p2, layout, theme, QRectF(0, 0, 400, 40), {}, -1, -1, {}, {}, -1, QStringLiteral("https://forge.ai"));
    p2.end();

    QVERIFY(normal != hovered);
}

void MarkdownCoreTests::coldLayoutCachesWidthInvariantsAndFastPaths()
{
    MarkdownParser parser;
    const MarkdownDocument document = parser.parse(QStringLiteral(
        "# Heading 1\n\n"
        "Short paragraph that fits without wrapping.\n\n"
        "```cpp\n"
        "int calculateHeight(int width) {\n"
        "    return width * 2;\n"
        "}\n"
        "```\n\n"
        "| Header A | Header B |\n"
        "|---|---|\n"
        "| Cell 1 | Cell 2 |\n"
        "| Cell 3 | Cell 4 |\n"
    ));
    const MarkdownTheme theme = MarkdownTheme::light();
    MarkdownLayoutEngine engine;

    // Pass 1: Cold width 800
    const DocumentLayout l1 = engine.layout(document, 800, theme);
    QVERIFY(!l1.blocks.isEmpty());
    const auto m1 = engine.metrics();
    QCOMPARE(m1.totalLayouts, 1ULL);
    QCOMPARE(m1.codeBlockCacheMisses, 1ULL);
    QCOMPARE(m1.codeBlockCacheHits, 0ULL);
    QVERIFY(m1.intrinsicFastPathHits > 0);

    // Pass 2: Cold width 750 (never seen before)
    const DocumentLayout l2 = engine.layout(document, 750, theme);
    QVERIFY(!l2.blocks.isEmpty());
    const auto m2 = engine.metrics();
    QCOMPARE(m2.totalLayouts, 2ULL);
    QCOMPARE(m2.codeBlockCacheMisses, 1ULL); // Miss count must NOT increase!
    QCOMPARE(m2.codeBlockCacheHits, 1ULL);   // Code block MUST hit cache!
    QVERIFY(m2.tableCellFastPathHits >= 4);   // Table cells must hit fast path!
    QVERIFY(m2.intrinsicFastPathHits > m1.intrinsicFastPathHits); // Headings/short paragraphs hit fast path!

    // Pass 3: Cold width 700 (never seen before)
    const DocumentLayout l3 = engine.layout(document, 700, theme);
    QVERIFY(!l3.blocks.isEmpty());
    const auto m3 = engine.metrics();
    QCOMPARE(m3.totalLayouts, 3ULL);
    QCOMPARE(m3.codeBlockCacheMisses, 1ULL);
    QCOMPARE(m3.codeBlockCacheHits, 2ULL);
}

namespace {

struct FrameDetail {
    int frameIndex = 0;
    qint64 timestampMs = 0;
    qint64 intervalUs = 0;
    qint64 durationUs = 0;
    QSize windowSize;
    int activeCards = 0;
    int cardGeometryChanges = 0;
    int maxPassesPerCard = 0;
    int markdownFullParsesDelta = 0;
    int markdownStableLayoutsDelta = 0;
    int markdownTailLayoutsDelta = 0;
};

struct ResizeBenchmarkMetrics {
    quint64 resizeEvents = 0;
    quint64 coalesceableEvents = 0;
    quint64 setGeometryEvents = 0;
    quint64 paintEvents = 0;
    qint64 totalResizeUs = 0;
    qint64 minResizeUs = -1;
    qint64 maxResizeUs = 0;
    int maxGeometryPassesPerCardInSingleFrame = 0;
    QVector<double> latencyHistoryMs;
    QVector<FrameDetail> frameDetails;
    QVector<FrameDetail> topWorstFrames;

    void recordFrame(const FrameDetail& frame) {
        ++resizeEvents;
        totalResizeUs += frame.durationUs;
        if (minResizeUs < 0 || frame.durationUs < minResizeUs) minResizeUs = frame.durationUs;
        if (frame.durationUs > maxResizeUs) maxResizeUs = frame.durationUs;
        if (frame.intervalUs > 0 && frame.intervalUs < 16000) ++coalesceableEvents;
        if (frame.maxPassesPerCard > maxGeometryPassesPerCardInSingleFrame) {
            maxGeometryPassesPerCardInSingleFrame = frame.maxPassesPerCard;
        }
        latencyHistoryMs.push_back(frame.durationUs / 1000.0);
        frameDetails.push_back(frame);

        topWorstFrames.push_back(frame);
        std::sort(topWorstFrames.begin(), topWorstFrames.end(), [](const FrameDetail& a, const FrameDetail& b) {
            return a.durationUs > b.durationUs;
        });
        if (topWorstFrames.size() > 5) topWorstFrames.resize(5);
    }

    double avgMs() const {
        return resizeEvents > 0 ? (totalResizeUs / (1000.0 * resizeEvents)) : 0.0;
    }

    double p95Ms() const {
        if (latencyHistoryMs.isEmpty()) return 0.0;
        QVector<double> sorted = latencyHistoryMs;
        std::sort(sorted.begin(), sorted.end());
        const int idx = qMin(sorted.size() - 1, static_cast<int>(sorted.size() * 0.95));
        return sorted.at(idx);
    }

    double coalescePotentialPercent() const {
        return resizeEvents > 1 ? (100.0 * coalesceableEvents / (resizeEvents - 1)) : 0.0;
    }
};

class ResizeHotspotFilter;

class BenchmarkMessageListView : public ui::widget::message::MessageListView {
public:
    using MessageListView::MessageListView;
    void setFilter(ResizeHotspotFilter* filter);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ResizeHotspotFilter* m_filter = nullptr;
};

class ResizeHotspotFilter : public QObject {
public:
    explicit ResizeHotspotFilter(ui::widget::message::MessageListView* listView, QLabel* hudLabel, QObject* parent = nullptr)
        : QObject(parent), m_listView(listView), m_hudLabel(hudLabel)
    {
        m_idleTimer = new QTimer(this);
        m_idleTimer->setSingleShot(true);
        m_idleTimer->setInterval(600);
        connect(m_idleTimer, &QTimer::timeout, this, &ResizeHotspotFilter::dumpSessionSummary);
    }

    void resetMetrics() {
        m_metrics = {};
        m_sessionStartTimer.invalidate();
        m_lastEventTimeUs = 0;
        updateHud();
    }

    ResizeBenchmarkMetrics metrics() const { return m_metrics; }

    void onResizeBegin() {
        if (!m_sessionStartTimer.isValid()) m_sessionStartTimer.start();
        const qint64 nowUs = m_sessionStartTimer.nsecsElapsed() / 1000;
        m_currentIntervalUs = m_lastEventTimeUs > 0 ? (nowUs - m_lastEventTimeUs) : 0;
        m_lastEventTimeUs = nowUs;
        m_currentFrameTimestampMs = nowUs / 1000;

        m_mdFullParsesBefore = 0;
        m_mdStableLayoutsBefore = 0;
        m_mdTailLayoutsBefore = 0;
        const auto markdownViews = m_listView->findChildren<ui::widget::MarkdownView*>();
        for (auto* v : markdownViews) {
            const auto m = v->metrics();
            m_mdFullParsesBefore += m.fullParseCount;
            m_mdStableLayoutsBefore += m.stableLayoutCount;
            m_mdTailLayoutsBefore += m.tailLayoutCount;
        }

        m_inResizePass = true;
        m_frameCardGeometryCounts.clear();
    }

    void onResizeEnd(QWidget* widget, qint64 durationUs) {
        m_inResizePass = false;

        int mdFullParsesAfter = 0;
        int mdStableLayoutsAfter = 0;
        int mdTailLayoutsAfter = 0;
        const auto markdownViews = m_listView->findChildren<ui::widget::MarkdownView*>();
        for (auto* v : markdownViews) {
            const auto m = v->metrics();
            mdFullParsesAfter += m.fullParseCount;
            mdStableLayoutsAfter += m.stableLayoutCount;
            mdTailLayoutsAfter += m.tailLayoutCount;
        }

        int maxPasses = 0;
        int frameGeometryChanges = 0;
        for (auto count : m_frameCardGeometryCounts) {
            frameGeometryChanges += count;
            maxPasses = qMax(maxPasses, count);
        }

        FrameDetail frame;
        frame.frameIndex = static_cast<int>(m_metrics.resizeEvents + 1);
        frame.timestampMs = m_currentFrameTimestampMs;
        frame.intervalUs = m_currentIntervalUs;
        frame.durationUs = durationUs;
        if (widget) {
            frame.windowSize = widget->size();
        }
        frame.activeCards = m_listView->activeCardCount();
        frame.cardGeometryChanges = frameGeometryChanges;
        frame.maxPassesPerCard = maxPasses;
        frame.markdownFullParsesDelta = mdFullParsesAfter - m_mdFullParsesBefore;
        frame.markdownStableLayoutsDelta = mdStableLayoutsAfter - m_mdStableLayoutsBefore;
        frame.markdownTailLayoutsDelta = mdTailLayoutsAfter - m_mdTailLayoutsBefore;

        m_metrics.recordFrame(frame);
        m_idleTimer->start();
        updateHud();
    }

    void dumpSessionSummary() {
        if (m_metrics.resizeEvents == 0) return;

        int totalFullParses = 0;
        int totalStableLayouts = 0;
        int totalTailLayouts = 0;
        const auto markdownViews = m_listView->findChildren<ui::widget::MarkdownView*>();
        for (auto* view : markdownViews) {
            const auto m = view->metrics();
            totalFullParses += m.fullParseCount;
            totalStableLayouts += m.stableLayoutCount;
            totalTailLayouts += m.tailLayoutCount;
        }

        const qint64 durationMs = m_sessionStartTimer.elapsed();
        const QString logPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resize_hotspot_profile.log"));

        qInfo().noquote() << QStringLiteral(
            "\n======================= [Resize Hotspot Session Summary] =======================\n"
            "Active Scenario: %1\n"
            "Message Count: %2 | Active Cards: %3 | Markdown Views: %4\n"
            "Session Duration: %5 ms | Resize Events: %6\n\n"
            "[1. Latency & Frame Cost]\n"
            "  Avg Resize Pass  : %7 ms\n"
            "  Min Resize Pass  : %8 ms\n"
            "  Max (Worst Frame): %9 ms\n"
            "  P95 Resize Pass  : %10 ms\n\n"
            "[2. Hotspots & Churn Indicators]\n"
            "  Total Card setGeometry Calls   : %11 (avg %12 / resize, max single-card churn: %13/frame)\n"
            "  Card / Viewport Paint Events   : %14\n"
            "  Total Markdown Full Parses     : %15\n"
            "  Total Markdown Layouts         : %16 (Stable) / %17 (Tail)\n\n"
            "[3. Coalescing Potential (16ms Frame Merge)]\n"
            "  Events within <16ms of previous: %18 / %19 (%20% can be coalesced)\n\n"
            "-> [Detailed Diagnostic Log Saved To]:\n"
            "   %21\n"
            "================================================================================"
        ).arg(m_scenarioName)
         .arg(m_listView->messageCount())
         .arg(m_listView->activeCardCount())
         .arg(markdownViews.size())
         .arg(durationMs)
         .arg(m_metrics.resizeEvents)
         .arg(m_metrics.avgMs(), 0, 'f', 2)
         .arg(m_metrics.minResizeUs >= 0 ? m_metrics.minResizeUs / 1000.0 : 0.0, 0, 'f', 2)
         .arg(m_metrics.maxResizeUs / 1000.0, 0, 'f', 2)
         .arg(m_metrics.p95Ms(), 0, 'f', 2)
         .arg(m_metrics.setGeometryEvents)
         .arg(m_metrics.resizeEvents > 0 ? (double)m_metrics.setGeometryEvents / m_metrics.resizeEvents : 0.0, 0, 'f', 2)
         .arg(m_metrics.maxGeometryPassesPerCardInSingleFrame)
         .arg(m_metrics.paintEvents)
         .arg(totalFullParses)
         .arg(totalStableLayouts)
         .arg(totalTailLayouts)
         .arg(m_metrics.coalesceableEvents)
         .arg(m_metrics.resizeEvents > 1 ? (m_metrics.resizeEvents - 1) : 0)
         .arg(m_metrics.coalescePotentialPercent(), 0, 'f', 1)
         .arg(logPath);

        QFile file(logPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << QStringLiteral("\n================================================================================\n");
            out << QStringLiteral("[SESSION TIMESTAMP]: ") << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << QStringLiteral("\n");
            out << QStringLiteral("Scenario: ") << m_scenarioName << QStringLiteral("\n");
            out << QStringLiteral("Message Count: ") << m_listView->messageCount()
                << QStringLiteral(" | Active Cards: ") << m_listView->activeCardCount()
                << QStringLiteral(" | Markdown Views: ") << markdownViews.size() << QStringLiteral("\n");
            out << QStringLiteral("Session Duration: ") << durationMs << QStringLiteral(" ms | Total Resize Events: ") << m_metrics.resizeEvents << QStringLiteral("\n");
            out << QStringLiteral("Avg Frame: ") << QString::number(m_metrics.avgMs(), 'f', 2)
                << QStringLiteral(" ms | Min: ") << QString::number(m_metrics.minResizeUs / 1000.0, 'f', 2)
                << QStringLiteral(" ms | Max: ") << QString::number(m_metrics.maxResizeUs / 1000.0, 'f', 2)
                << QStringLiteral(" ms | P95: ") << QString::number(m_metrics.p95Ms(), 'f', 2) << QStringLiteral(" ms\n");
            out << QStringLiteral("Total setGeometry: ") << m_metrics.setGeometryEvents
                << QStringLiteral(" | Max setGeometry/Card/Frame: ") << m_metrics.maxGeometryPassesPerCardInSingleFrame
                << QStringLiteral(" | Total Paints: ") << m_metrics.paintEvents << QStringLiteral("\n");
            out << QStringLiteral("Markdown Parses: ") << totalFullParses
                << QStringLiteral(" | Stable Layouts: ") << totalStableLayouts
                << QStringLiteral(" | Tail Layouts: ") << totalTailLayouts << QStringLiteral("\n");
            out << QStringLiteral("Coalescing Potential: ") << m_metrics.coalesceableEvents << QStringLiteral("/")
                << (m_metrics.resizeEvents > 1 ? m_metrics.resizeEvents - 1 : 0)
                << QStringLiteral(" (") << QString::number(m_metrics.coalescePotentialPercent(), 'f', 1) << QStringLiteral("%)\n\n");

            out << QStringLiteral("--- [TOP 5 SLOWEST (WORST) FRAMES] ---\n");
            for (int i = 0; i < m_metrics.topWorstFrames.size(); ++i) {
                const auto& wf = m_metrics.topWorstFrames[i];
                out << QStringLiteral("  #%1: Frame #%2 | Latency: %3 ms | Window: %4x%5 | Cards: %6 | setGeometry: %7 | Inter-event interval: %8 ms\n")
                    .arg(i + 1)
                    .arg(wf.frameIndex)
                    .arg(wf.durationUs / 1000.0, 0, 'f', 2)
                    .arg(wf.windowSize.width())
                    .arg(wf.windowSize.height())
                    .arg(wf.activeCards)
                    .arg(wf.cardGeometryChanges)
                    .arg(wf.intervalUs / 1000.0, 0, 'f', 2);
            }

            out << QStringLiteral("\n--- [FRAME-BY-FRAME TIMELINE (First 100 Frames)] ---\n");
            out << QStringLiteral("Index | Delta (ms) | Interval (ms) | Win Width x Height | Active Cards | setGeometry | MD Layouts\n");
            const int sampleCount = qMin<int>(m_metrics.frameDetails.size(), 100);
            for (int i = 0; i < sampleCount; ++i) {
                const auto& fd = m_metrics.frameDetails[i];
                out << QStringLiteral("%1 | %2 ms | %3 ms | %4x%5 | %6 | %7 | +%8\n")
                    .arg(fd.frameIndex, 5)
                    .arg(fd.durationUs / 1000.0, 8, 'f', 2)
                    .arg(fd.intervalUs / 1000.0, 8, 'f', 2)
                    .arg(fd.windowSize.width(), 4)
                    .arg(fd.windowSize.height(), 4)
                    .arg(fd.activeCards, 4)
                    .arg(fd.cardGeometryChanges, 4)
                    .arg(fd.markdownStableLayoutsDelta + fd.markdownTailLayoutsDelta, 3);
            }
            if (m_metrics.frameDetails.size() > 100) {
                out << QStringLiteral("... (%1 more frames recorded in session)\n").arg(m_metrics.frameDetails.size() - 100);
            }
            out << QStringLiteral("================================================================================\n\n");
            file.close();
        }
    }

    void setScenarioName(const QString& name) {
        m_scenarioName = name;
        resetMetrics();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            if (watched->isWidgetType() && watched != m_listView) {
                ++m_metrics.setGeometryEvents;
                if (m_inResizePass) {
                    m_frameCardGeometryCounts[watched]++;
                }
            }
        } else if (event->type() == QEvent::Paint) {
            ++m_metrics.paintEvents;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void updateHud() {
        if (!m_hudLabel) return;
        m_hudLabel->setText(QStringLiteral(
            "<b>场景:</b> %1 | <b>消息:</b> %2 (活跃卡片: %3) | <b>Resize事件:</b> %4 | "
            "<b>平均耗时:</b> <font color='%5'>%6 ms</font> | <b>P95:</b> %7 ms | <b>几何更新:</b> %8 | "
            "<b>合并潜力:</b> %9%"
        ).arg(m_scenarioName)
         .arg(m_listView->messageCount())
         .arg(m_listView->activeCardCount())
         .arg(m_metrics.resizeEvents)
         .arg(m_metrics.avgMs() > 16.0 ? QStringLiteral("#d9534f") : QStringLiteral("#5cb85c"))
         .arg(m_metrics.avgMs(), 0, 'f', 2)
         .arg(m_metrics.p95Ms(), 0, 'f', 2)
         .arg(m_metrics.setGeometryEvents)
         .arg(m_metrics.coalescePotentialPercent(), 0, 'f', 1));
    }

    ui::widget::message::MessageListView* m_listView = nullptr;
    QLabel* m_hudLabel = nullptr;
    ResizeBenchmarkMetrics m_metrics;
    QElapsedTimer m_sessionStartTimer;
    qint64 m_lastEventTimeUs = 0;
    qint64 m_currentIntervalUs = 0;
    qint64 m_currentFrameTimestampMs = 0;
    int m_mdFullParsesBefore = 0;
    int m_mdStableLayoutsBefore = 0;
    int m_mdTailLayoutsBefore = 0;
    bool m_inResizePass = false;
    QHash<QObject*, int> m_frameCardGeometryCounts;
    QTimer* m_idleTimer = nullptr;
    QString m_scenarioName = QStringLiteral("D. cmark-gfm 全特性全景展示");
};

void BenchmarkMessageListView::setFilter(ResizeHotspotFilter* filter)
{
    m_filter = filter;
}

void BenchmarkMessageListView::resizeEvent(QResizeEvent* event)
{
    if (m_filter) m_filter->onResizeBegin();
    QElapsedTimer timer;
    timer.start();
    ui::widget::message::MessageListView::resizeEvent(event);
    const qint64 elapsedUs = timer.nsecsElapsed() / 1000;
    if (m_filter) m_filter->onResizeEnd(this, elapsedUs);
}

static QList<domain::conversation::Message> createScenarioA() {
    QList<domain::conversation::Message> list;
    for (int i = 0; i < 20; ++i) {
        domain::conversation::Message msg;
        msg.id = QUuid::createUuid();
        msg.role = (i % 2 == 0) ? domain::MessageRole::User : domain::MessageRole::Assistant;
        const QString text = (i % 2 == 0)
            ? QStringLiteral("用户提问 %1：请问如何使用 Qt 设计高性能的聊天界面？").arg(i / 2 + 1)
            : QStringLiteral("助手回答 %1：使用 QListView 或虚拟化卡片，结合 QTextLayout 分离排版与渲染即可。").arg(i / 2 + 1);
        msg.blocks.append(domain::conversation::MessageBlock{domain::BlockType::Text, domain::conversation::TextBlock{text}});
        list.append(std::move(msg));
    }
    return list;
}

static QList<domain::conversation::Message> createScenarioB() {
    QList<domain::conversation::Message> list;
    for (int i = 0; i < 200; ++i) {
        domain::conversation::Message msg;
        msg.id = QUuid::createUuid();
        msg.role = (i % 2 == 0) ? domain::MessageRole::User : domain::MessageRole::Assistant;
        QString text;
        if (i % 2 == 0) {
            text = QStringLiteral("### 提问 #%1\n在开发 Qt 应用程序时，`QWidget::resizeEvent` 高频触发有哪些优化手段？").arg(i + 1);
        } else {
            text = QStringLiteral("这是第 %1 条回复。\n\n- 使用 16ms 定时器合并 ResizeEvent 达到帧对齐\n- 降低不可见区域的 Preload Viewport 数量\n- 为 Markdown排版增加宽度感知（width-aware）缓存").arg(i + 1);
        }
        msg.blocks.append(domain::conversation::MessageBlock{domain::BlockType::Text, domain::conversation::TextBlock{text}});
        list.append(std::move(msg));
    }
    return list;
}

static QList<domain::conversation::Message> createScenarioC() {
    QList<domain::conversation::Message> list;
    for (int i = 0; i < 200; ++i) {
        domain::conversation::Message msg;
        msg.id = QUuid::createUuid();
        msg.role = (i % 2 == 0) ? domain::MessageRole::User : domain::MessageRole::Assistant;
        QString text;
        if (i % 2 == 0) {
            text = QStringLiteral("请给出一份完整的 C++ 代码和性能对比表格，用于说明虚拟化列表的架构设计。");
        } else if (i % 10 == 1) {
            text = QStringLiteral(
                "# 深入解析 Qt 虚拟列表与 Markdown 渲染引擎\n\n"
                "在现代客户端应用开发中，**高频窗口缩放**与**大量富文本流式输出**对 GUI 框架提出了极高的要求。\n\n"
                "## 架构对比表\n\n"
                "| 阶段 | 传统方案 | 优化方案 | 性能提升 |\n"
                "|---|---|---|---|\n"
                "| 事件处理 | 每次 Resize 同步重排 | 16ms 帧合并调度 | 60% CPU 降低 |\n"
                "| 测量范围 | 全量 2000 项或 5 视口 | 仅视口可见卡片 | 85% 测量减少 |\n"
                "| 布局缓存 | 单一槽位或无缓存 | LRU Width-Aware 缓存 | 90% 重排消除 |\n"
                "| 文本绘制 | QTextDocument | QTextLayout + Block Culling | 70% 渲染加速 |\n\n"
                "## 核心 C++ 实现示例\n\n"
                "```cpp\n"
                "#include <QTextLayout>\n"
                "#include <QPainter>\n\n"
                "class OptimizedMessageCard {\n"
                "public:\n"
                "    void setAvailableWidth(int width) {\n"
                "        if (m_width == width) return;\n"
                "        m_width = width;\n"
                "        m_height = measureHeight(width);\n"
                "    }\n"
                "    int height() const { return m_height; }\n"
                "private:\n"
                "    int m_width = 0;\n"
                "    int m_height = 0;\n"
                "};\n"
                "```\n\n"
                "### 任务检查清单\n\n"
                "- [x] 建立基准测试与热点分析工具\n"
                "- [ ] 实现 16ms Resize 帧合并\n"
                "- [ ] 降低 Interactive Resize 的预加载视口\n"
                "- [ ] 接入 Width-Aware 测量缓存\n\n"
                "> 提示：通过解耦 `heightForWidth` 测量与实际 `setGeometry`，可以彻底消灭反复重绘震荡。"
            );
        } else {
            text = QStringLiteral("普通消息 %1：包含 `QTextLayout`、`QPainter` 与 `ScrollView` 的综合运用。").arg(i + 1);
        }
        msg.blocks.append(domain::conversation::MessageBlock{domain::BlockType::Text, domain::conversation::TextBlock{text}});
        list.append(std::move(msg));
    }
    return list;
}

static QList<domain::conversation::Message> createScenarioAllCmarkGfmFeatures() {
    QList<domain::conversation::Message> list;

    // User prompt
    {
        domain::conversation::Message msg;
        msg.id = QUuid::createUuid();
        msg.role = domain::MessageRole::User;
        msg.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text,
            domain::conversation::TextBlock{QStringLiteral("请展示 cmark-gfm 支持的所有 Markdown 语法和扩展特性的完整渲染效果。")}
        });
        list.append(std::move(msg));
    }

    // Assistant mega-document with all cmark-gfm features
    {
        domain::conversation::Message msg;
        msg.id = QUuid::createUuid();
        msg.role = domain::MessageRole::Assistant;
        msg.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text,
            domain::conversation::TextBlock{generateLongMarkdownDocument()}
        });
        list.append(std::move(msg));
    }

    return list;
}

} // namespace

void MarkdownCoreTests::streamingIncrementalTextIsAlwaysCurrent()
{
    // streaming Incremental text should always render latest text, not stale cached inlines
    ui::widget::MarkdownDocumentController controller;
    ui::widget::MarkdownDocumentLayout layout(&controller);
    layout.setWidth(600);
    layout.setTheme(MarkdownTheme::light());
    
    controller.beginStream();
    controller.appendMarkdown(QStringLiteral("Prefix"));
    auto doc1 = layout.currentLayout();
    QVERIFY(doc1 && !doc1->blocks.isEmpty());
    QCOMPARE(doc1->blocks.front().inlineLayout->text, QStringLiteral("Prefix"));
    
    // Append more text, making it a new inline in the same paragraph
    controller.appendMarkdown(QStringLiteral("Suffix"));
    auto doc2 = layout.currentLayout();
    QVERIFY(doc2 && !doc2->blocks.isEmpty());
    QCOMPARE(doc2->blocks.front().inlineLayout->text, QStringLiteral("PrefixSuffix"));
    controller.finishStream();
}

void MarkdownCoreTests::streamingTailTaskCheckRectIsInsideBlockRect()
{
    // streaming Tail merge block missing translate
    ui::widget::MarkdownDocumentController controller;
    ui::widget::MarkdownDocumentLayout layout(&controller);
    layout.setWidth(600);
    layout.setTheme(MarkdownTheme::light());
    
    controller.beginStream();
    controller.appendMarkdown(QStringLiteral("# Title\n\n")); // Stable part
    controller.appendMarkdown(QStringLiteral("- [ ] Task 1")); // Tail part
    
    auto doc = layout.currentLayout();
    QVERIFY(doc && doc->blocks.size() >= 2);
    const auto& taskBlock = doc->blocks.back();
    QCOMPARE(taskBlock.kind, BlockKind::ListItem);
    QVERIFY(taskBlock.taskItem);
    
    // Check if taskCheckRect is translated properly
    QVERIFY(taskBlock.taskCheckRect.top() >= taskBlock.rect.top());
    QVERIFY(taskBlock.taskCheckRect.bottom() <= taskBlock.rect.bottom());
    controller.finishStream();
}

void MarkdownCoreTests::streamingTailCodeScrollInfoIsConsistentWithBlockRect()
{
    // streaming Tail code block missing scrollInfo translate
    ui::widget::MarkdownDocumentController controller;
    ui::widget::MarkdownDocumentLayout layout(&controller);
    layout.setWidth(600);
    layout.setTheme(MarkdownTheme::light());
    
    controller.beginStream();
    controller.appendMarkdown(QStringLiteral("# Title\n\n")); // Stable part
    controller.appendMarkdown(QStringLiteral("```\nCode\n```")); // Tail part
    
    auto doc = layout.currentLayout();
    QVERIFY(doc && doc->blocks.size() >= 2);
    const auto& codeBlock = doc->blocks.back();
    QCOMPARE(codeBlock.kind, BlockKind::CodeBlock);
    
    // Check if scrollInfo viewportRect is translated properly
    QVERIFY(codeBlock.scrollInfo.viewportRect.top() >= codeBlock.rect.top());
    QVERIFY(codeBlock.scrollInfo.viewportRect.bottom() <= codeBlock.rect.bottom());
    controller.finishStream();
}

void MarkdownCoreTests::streamingStableImageHeightDoesNotRegressOnNextToken()
{
    // Stable image height regression when appending next tokens
    ui::widget::MarkdownDocumentController controller;
    ui::widget::MarkdownDocumentLayout layout(&controller);
    layout.setWidth(600);
    layout.setTheme(MarkdownTheme::light());
    
    controller.beginStream();
    controller.appendMarkdown(QStringLiteral("![img](test.png)\n\n")); // Will become stable
    
    // Simulate image loaded
    layout.updateImageSize(QStringLiteral("test.png"), QSize(200, 100));
    auto doc1 = layout.currentLayout();
    QVERIFY(doc1 && !doc1->blocks.isEmpty());
    const qreal h1 = doc1->blocks.front().rect.height();
    
    // Append more text (triggers tail rebuild and relayout)
    controller.appendMarkdown(QStringLiteral("More text"));
    auto doc2 = layout.currentLayout();
    QVERIFY(doc2 && !doc2->blocks.isEmpty());
    const qreal h2 = doc2->blocks.front().rect.height();
    
    // Height should remain the patched size, not revert to intrinsic
    QCOMPARE(h1, h2);
    controller.finishStream();
}

void MarkdownCoreTests::streamingFinishGeometryMatchesFinalLayout()
{
    // Final tail merge layout should be identical to non-streaming full relayout
    ui::widget::MarkdownDocumentController controller;
    ui::widget::MarkdownDocumentLayout layout(&controller);
    layout.setWidth(600);
    layout.setTheme(MarkdownTheme::light());
    
    controller.beginStream();
    controller.appendMarkdown(QStringLiteral("# Header\n\n- List 1\n- List 2\n\n---\n\nParagraph."));
    auto streamingDoc = layout.currentLayout();
    controller.finishStream();
    auto finalDoc = layout.currentLayout();
    
    QVERIFY(streamingDoc);
    QVERIFY(finalDoc);
    QCOMPARE(streamingDoc->blocks.size(), finalDoc->blocks.size());
    for (qsizetype i = 0; i < streamingDoc->blocks.size(); ++i) {
        QCOMPARE(streamingDoc->blocks[i].rect, finalDoc->blocks[i].rect);
    }
}

void MarkdownCoreTests::streamingFinishedEmittedAfterFinalLayout()
{
    // streamingFinished must be emitted AFTER the layout is rebuilt
    ui::widget::MarkdownView view;
    QSignalSpy spyReady(&view, &ui::widget::MarkdownView::documentSizeChanged);
    QSignalSpy spyFinished(&view, &ui::widget::MarkdownView::streamingFinished);
    
    view.beginStream();
    view.appendStreamingText(QStringLiteral("Content"));
    spyReady.clear();
    
    view.finishStreaming();
    // Finish should trigger a rebuild (layoutReady -> documentSizeChanged) then streamingFinished
    QVERIFY(spyReady.count() > 0 || view.metrics().blockCount > 0);
    QCOMPARE(spyFinished.count(), 1);
}

void MarkdownCoreTests::fencedCodeNotClosedByDifferentMarker()
{
    // A fenced code block starting with ` should not be closed by ~
    ui::widget::MarkdownDocumentController controller;
    controller.beginStream();
    // ```python
    controller.appendMarkdown(QStringLiteral("```python\n"));
    QCOMPARE(controller.stableParseCount(), 0); // No stable boundary yet
    // ~~~
    controller.appendMarkdown(QStringLiteral("~~~\n"));
    QCOMPARE(controller.stableParseCount(), 0); // Still in fence, no boundary
    // ```
    controller.appendMarkdown(QStringLiteral("```\n\nText"));
    QVERIFY(controller.stableParseCount() > 0); // Now it's closed and a boundary can form
    controller.finishStream();
}

void MarkdownCoreTests::visualTest()
{
    const QString logPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resize_hotspot_profile.log"));
    QFile::remove(logPath);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("MessageListView 窗口缩放性能热点测试 [ForgeAI Benchmark & cmark-gfm Showcase]"));
    window.resize(980, 720);

    auto* central = new QWidget(&window);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    auto* hudLabel = new QLabel(central);
    hudLabel->setStyleSheet(QStringLiteral("background: #2b2b2b; color: #ffffff; padding: 8px; border-radius: 6px; font-family: 'Segoe UI', sans-serif; font-size: 13px;"));
    mainLayout->addWidget(hudLabel);

    auto* toolLayout = new QHBoxLayout();
    auto* btnA = new QPushButton(QStringLiteral("A. 20条短消息"), central);
    auto* btnB = new QPushButton(QStringLiteral("B. 200条普通消息"), central);
    auto* btnC = new QPushButton(QStringLiteral("C. 200条+长Markdown"), central);
    auto* btnD = new QPushButton(QStringLiteral("D. cmark-gfm 全特性全景"), central);
    auto* btnE = new QPushButton(QStringLiteral("E. 短流式+实时Resize"), central);
    auto* btnF = new QPushButton(QStringLiteral("F. 5000字长文本高频流式+Resize"), central);
    auto* btnReset = new QPushButton(QStringLiteral("重置统计"), central);
    toolLayout->addWidget(btnA);
    toolLayout->addWidget(btnB);
    toolLayout->addWidget(btnC);
    toolLayout->addWidget(btnD);
    toolLayout->addWidget(btnE);
    toolLayout->addWidget(btnF);
    toolLayout->addWidget(btnReset);
    toolLayout->addStretch();
    mainLayout->addLayout(toolLayout);

    auto* listView = new BenchmarkMessageListView(central);
    mainLayout->addWidget(listView, 1);
    window.setCentralWidget(central);

    auto* filter = new ResizeHotspotFilter(listView, hudLabel, &window);
    listView->setFilter(filter);
    listView->installEventFilter(filter);
    if (listView->viewport()) listView->viewport()->installEventFilter(filter);

    auto* streamTimer = new QTimer(&window);
    streamTimer->setInterval(35);

    auto loadScenario = [&](const QString& name, const QList<domain::conversation::Message>& msgs) {
        streamTimer->stop();
        filter->setScenarioName(name);
        listView->syncMessages(msgs);
        QCoreApplication::processEvents();
    };

    QObject::connect(btnA, &QPushButton::clicked, [&] { loadScenario(QStringLiteral("A. 20条短消息"), createScenarioA()); });
    QObject::connect(btnB, &QPushButton::clicked, [&] { loadScenario(QStringLiteral("B. 200条普通消息"), createScenarioB()); });
    QObject::connect(btnC, &QPushButton::clicked, [&] { loadScenario(QStringLiteral("C. 200条 + 长Markdown (高负载)"), createScenarioC()); });
    QObject::connect(btnD, &QPushButton::clicked, [&] { loadScenario(QStringLiteral("D. cmark-gfm 全特性全景展示"), createScenarioAllCmarkGfmFeatures()); });
    QObject::connect(btnReset, &QPushButton::clicked, [&] { filter->resetMetrics(); });

    QObject::connect(btnE, &QPushButton::clicked, [&] {
        QList<domain::conversation::Message> msgs;
        {
            domain::conversation::Message userMsg;
            userMsg.id = QUuid::createUuid();
            userMsg.role = domain::MessageRole::User;
            userMsg.blocks.append(domain::conversation::MessageBlock{
                domain::BlockType::Text,
                domain::conversation::TextBlock{QStringLiteral("请实时流式演示一段包含代码的 Markdown 回复。")}
            });
            msgs.append(userMsg);
        }
        domain::conversation::Message streamMsg;
        streamMsg.id = QUuid::createUuid();
        streamMsg.role = domain::MessageRole::Assistant;
        streamMsg.status = domain::MessageStatus::Sending;
        streamMsg.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text,
            domain::conversation::TextBlock{QString()}
        });
        msgs.append(streamMsg);
        loadScenario(QStringLiteral("E. 短流式 + 实时Resize"), msgs);

        static const QStringList tokens = {
            QStringLiteral("在"), QStringLiteral("现代"), QStringLiteral("客户端"), QStringLiteral("开发"), QStringLiteral("中，"),
            QStringLiteral("Markdown"), QStringLiteral(" 渲染"), QStringLiteral("引擎"), QStringLiteral("需要"), QStringLiteral("支持"),
            QStringLiteral(" 高频"), QStringLiteral("的"), QStringLiteral("窗口"), QStringLiteral("拖动"), QStringLiteral("缩放。\n\n"),
            QStringLiteral("```cpp\n"), QStringLiteral("void onTokenStream() {\n"), QStringLiteral("    relayoutTail();\n"),
            QStringLiteral("}\n```\n\n"), QStringLiteral("自研"), QStringLiteral("的分层"), QStringLiteral("排版"), QStringLiteral("引擎"),
            QStringLiteral("将 Stable Document"), QStringLiteral(" 与 Tail Document"), QStringLiteral(" 隔离，"),
            QStringLiteral("实现 O(1) 复杂度的"), QStringLiteral("流式增量追加与尾部排版。")
        };

        streamTimer->disconnect();
        streamTimer->setInterval(35);
        auto tokenIdx = std::make_shared<int>(0);
        auto sessionMsgs = std::make_shared<QList<domain::conversation::Message>>(msgs);
        auto accumulatedText = std::make_shared<QString>();

        QObject::connect(streamTimer, &QTimer::timeout, [listView, tokenIdx, sessionMsgs, accumulatedText, streamTimer]() {
            if (*tokenIdx >= tokens.size()) {
                streamTimer->stop();
                (*sessionMsgs)[1].status = domain::MessageStatus::Sent;
                listView->syncMessages(*sessionMsgs);
                return;
            }
            *accumulatedText += tokens.at(*tokenIdx);
            (*tokenIdx)++;
            (*sessionMsgs)[1].blocks[0] = domain::conversation::MessageBlock{
                domain::BlockType::Text,
                domain::conversation::TextBlock{*accumulatedText}
            };
            listView->syncMessages(*sessionMsgs);
            listView->scrollToBottom();
        });
        streamTimer->start();
    });

    QObject::connect(btnF, &QPushButton::clicked, [&] {
        QList<domain::conversation::Message> msgs;
        {
            domain::conversation::Message userMsg;
            userMsg.id = QUuid::createUuid();
            userMsg.role = domain::MessageRole::User;
            userMsg.blocks.append(domain::conversation::MessageBlock{
                domain::BlockType::Text,
                domain::conversation::TextBlock{QStringLiteral("请开始流式生成 5000 字全特性 Markdown 深度技术文档。")}
            });
            msgs.append(userMsg);
        }
        domain::conversation::Message streamMsg;
        streamMsg.id = QUuid::createUuid();
        streamMsg.role = domain::MessageRole::Assistant;
        streamMsg.status = domain::MessageStatus::Sending;
        streamMsg.blocks.append(domain::conversation::MessageBlock{
            domain::BlockType::Text,
            domain::conversation::TextBlock{QString()}
        });
        msgs.append(streamMsg);
        loadScenario(QStringLiteral("F. 5000字长文本高频流式 + 实时Resize"), msgs);

        const QString fullDoc = generateLongMarkdownDocument();
        QStringList chunks;
        int pos = 0;
        int step = 14;
        while (pos < fullDoc.length()) {
            const int chunkSize = qMin(step, static_cast<int>(fullDoc.length()) - pos);
            chunks.append(fullDoc.mid(pos, chunkSize));
            pos += chunkSize;
            step = (step % 17) + 6;
        }

        streamTimer->disconnect();
        streamTimer->setInterval(20);
        auto tokenIdx = std::make_shared<int>(0);
        auto docChunks = std::make_shared<QStringList>(chunks);
        auto sessionMsgs = std::make_shared<QList<domain::conversation::Message>>(msgs);
        auto accumulatedText = std::make_shared<QString>();

        QObject::connect(streamTimer, &QTimer::timeout, [listView, tokenIdx, docChunks, sessionMsgs, accumulatedText, streamTimer]() {
            if (*tokenIdx >= docChunks->size()) {
                streamTimer->stop();
                (*sessionMsgs)[1].status = domain::MessageStatus::Sent;
                listView->syncMessages(*sessionMsgs);
                return;
            }
            *accumulatedText += docChunks->at(*tokenIdx);
            (*tokenIdx)++;
            (*sessionMsgs)[1].blocks[0] = domain::conversation::MessageBlock{
                domain::BlockType::Text,
                domain::conversation::TextBlock{*accumulatedText}
            };
            listView->syncMessages(*sessionMsgs);
            listView->scrollToBottom();
        });
        streamTimer->start();
    });

    // Default load Scenario D (All cmark-gfm features)
    loadScenario(QStringLiteral("D. cmark-gfm 全特性全景展示"), createScenarioAllCmarkGfmFeatures());

    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        window.show();
        for (int w = 600; w <= 800; w += 20) {
            window.resize(w, 500);
            QCoreApplication::processEvents();
        }
        filter->dumpSessionSummary();
        QVERIFY(filter->metrics().resizeEvents > 0);
    } else {
        window.show();
        window.raise();
        window.activateWindow();

        qInfo().noquote() << QStringLiteral(
            "\n[Benchmark Started] 窗口缩放性能热点测试窗口已启动。\n"
            "-> 当前默认加载: D. cmark-gfm 全特性全景展示\n"
            "-> 请拖动窗口边框调整大小，观察卡顿与排版表现。\n"
            "-> 可点击顶部按钮切换 A/B/C/D/E 五种测试场景。\n"
            "-> 控制台会在每次拖动暂停时输出详细热点耗时分析。\n"
            "-> 关闭窗口即可结束测试并输出总汇。"
        );

        QEventLoop loop;
        window.setAttribute(Qt::WA_DeleteOnClose, true);
        QObject::connect(&window, &QWidget::destroyed, &loop, &QEventLoop::quit);
        loop.exec();

        filter->dumpSessionSummary();
    }
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    MarkdownCoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "MarkdownCoreTests.moc"
