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
#include <memory>
#include <vector>

using namespace ui::markdown;

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
    void markdownViewThemeAndSelectionApi();
    void markdownViewFollowsFluentDarkTheme();
    void taskListsHaveStableHitTargetsAndOptionalInteraction();
    void virtualizesLargeDocumentPainting();
    void laysOutUnicodeAndHighDpiContent();
    void reportsManyViewConstructionCost();
    void virtualizesMessageCards();
    void virtualizesThousandsOfMessageRecords();
    void fitContentSizeHintIsInvariantUnderActualResizes();
    void heightForWidthRespondsToWrappingWidth();
    void autoFitHeightContractIsRespected();
    void inlineCodeSelectionPaintsSelectionBackground();
    void headingThemeColorIsApplied();
    void linkHoverHighlightIsApplied();
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

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    MarkdownCoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "MarkdownCoreTests.moc"
