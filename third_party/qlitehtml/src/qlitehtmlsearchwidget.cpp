#include "qlitehtmlsearchwidget.h"

#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTextBoundaryFinder>

#include "qlitehtmlwidget.h"

namespace {
bool isEmojiCodePoint(unsigned int codePoint)
{
    return codePoint == 0x20E3 || codePoint == 0x00A9 || codePoint == 0x00AE || codePoint == 0x203C
           || codePoint == 0x2049 || codePoint == 0x2122 || codePoint == 0x2139
           || (codePoint >= 0x2194 && codePoint <= 0x21AA)
           || (codePoint >= 0x231A && codePoint <= 0x2328) || codePoint == 0x23CF
           || (codePoint >= 0x23E9 && codePoint <= 0x23FA) || codePoint == 0x24C2
           || (codePoint >= 0x25AA && codePoint <= 0x25AB) || codePoint == 0x25B6
           || codePoint == 0x25C0 || (codePoint >= 0x25FB && codePoint <= 0x25FE)
           || (codePoint >= 0x2600 && codePoint <= 0x27BF)
           || (codePoint >= 0x2934 && codePoint <= 0x2935)
           || (codePoint >= 0x2B05 && codePoint <= 0x2B55) || codePoint == 0x3030
           || codePoint == 0x303D || codePoint == 0x3297 || codePoint == 0x3299
           || (codePoint >= 0x1F000 && codePoint <= 0x1FAFF);
}

int graphemeCount(const QString &text, int maxCount)
{
    if (text.isEmpty()) {
        return 0;
    }

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.toStart();

    int count = 0;
    while (finder.toNextBoundary() != -1) {
        ++count;

        if (count >= maxCount) {
            break;
        }
    }

    return count;
}

bool shouldStartSearch(const QString &text)
{
    const int minimumSearchLength = 2;
    const int count = graphemeCount(text, minimumSearchLength);
    if (count >= minimumSearchLength) {
        return true;
    }

    if (count != 1) {
        return false;
    }

    const auto codePoints = text.toUcs4();
    for (unsigned int codePoint : codePoints) {
        if (isEmojiCodePoint(codePoint)) {
            return true;
        }
    }

    return false;
}
} // namespace

QLiteHtmlSearchWidget::QLiteHtmlSearchWidget(QLiteHtmlWidget *parent)
    : QWidget(parent)
{
    _liteHtmlWidget = parent;
    hide();

    // UI built in code (equivalent of the removed qlitehtmlsearchwidget.ui).
    auto *gridLayout = new QGridLayout(this);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    m_closeButton = new QPushButton(this);
    m_closeButton->setToolTip(tr("Close search"));
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_closeButton->setFlat(true);
    gridLayout->addWidget(m_closeButton, 0, 0);

    auto *searchLabel = new QLabel(tr("Find:"), this);
    searchLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    gridLayout->addWidget(searchLabel, 0, 1);

    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText(tr("Find in text"));
    gridLayout->addWidget(m_searchLineEdit, 0, 2);

    m_searchDownButton = new QPushButton(this);
    m_searchDownButton->setToolTip(tr("Search forward"));
    m_searchDownButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_searchDownButton->setFlat(true);
    gridLayout->addWidget(m_searchDownButton, 0, 3);

    m_searchUpButton = new QPushButton(this);
    m_searchUpButton->setToolTip(tr("Search backward"));
    m_searchUpButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_searchUpButton->setFlat(true);
    gridLayout->addWidget(m_searchUpButton, 0, 4);

    m_modeComboBox = new QComboBox(this);
    m_modeComboBox->addItem(tr("Plain text"));
    m_modeComboBox->addItem(tr("Whole words"));
    m_modeComboBox->addItem(tr("Regular expression"));
    gridLayout->addWidget(m_modeComboBox, 0, 5);

    // The original .ui referenced icons from a media.qrc that is not part of
    // this project, so the button was blank; use a text label instead.
    m_matchCaseSensitiveButton = new QPushButton(this);
    m_matchCaseSensitiveButton->setToolTip(tr("Match case sensitive"));
    m_matchCaseSensitiveButton->setText(tr("Aa"));
    m_matchCaseSensitiveButton->setCheckable(true);
    m_matchCaseSensitiveButton->setFlat(true);
    gridLayout->addWidget(m_matchCaseSensitiveButton, 0, 6);

    QObject::connect(m_closeButton, &QPushButton::clicked, this, &QLiteHtmlSearchWidget::deactivate);
    QObject::connect(m_searchLineEdit,
                     &QLineEdit::textChanged,
                     this,
                     &QLiteHtmlSearchWidget::searchLineEditTextChanged);
    QObject::connect(m_searchDownButton, &QPushButton::clicked, this, &QLiteHtmlSearchWidget::doSearchDown);
    QObject::connect(m_searchUpButton, &QPushButton::clicked, this, &QLiteHtmlSearchWidget::doSearchUp);

    // Set up debounce timer so the search is delayed while the user is still
    // typing
    _debounceTimer.setSingleShot(true);
    _debounceTimer.setInterval(300);
    QObject::connect(&_debounceTimer, &QTimer::timeout, this, &QLiteHtmlSearchWidget::doSearchDown);

    installEventFilter(this);
    m_searchLineEdit->installEventFilter(this);

#ifdef Q_OS_MAC
    layout()->setSpacing(8);
    QString buttonStyle = "QPushButton {margin: 0}";
    m_closeButton->setStyleSheet(buttonStyle);
    m_searchDownButton->setStyleSheet(buttonStyle);
    m_searchUpButton->setStyleSheet(buttonStyle);
    m_matchCaseSensitiveButton->setStyleSheet(buttonStyle);
#endif
}

QLiteHtmlSearchWidget::~QLiteHtmlSearchWidget() = default;

void QLiteHtmlSearchWidget::activate()
{
    show();

    const int verticalScrollBarValue = _liteHtmlWidget->verticalScrollBar()->value();
    const int horizontalScrollBarValue = _liteHtmlWidget->horizontalScrollBar()->value();

    QString selectedText = _liteHtmlWidget->selectedText();
    // Preset the selected text as search text if there is any, replacing any
    // existing search text. Set _searchFromSelection BEFORE calling setText()
    // so the textChanged signal triggers doSearch() with incremental=true,
    // which makes the litehtml engine start from the beginning of the current
    // selection instead of after it — keeping the first result on the
    // originally selected word (for #3538).
    bool searchAlreadyDone = false;
    if (!selectedText.isEmpty()) {
        _searchFromSelection = true;
        m_searchLineEdit->setText(selectedText);
        // If the text actually changed, textChanged fired synchronously:
        // searchLineEditTextChanged -> doSearch() already ran with incremental=true
        // and found the correct occurrence. _searchFromSelection was consumed
        // (set to false) inside doSearch(). Skip the doSearchDown() below to
        // avoid advancing to the next occurrence.
        // If the text did NOT change (same search term as before), _searchFromSelection
        // is still true; we fall through so doSearchDown() uses it.
        searchAlreadyDone = !_searchFromSelection;
    }

    m_searchLineEdit->setFocus();
    m_searchLineEdit->selectAll();

    if (!searchAlreadyDone) {
        doSearchDown();
    }

    _liteHtmlWidget->verticalScrollBar()->setValue(verticalScrollBarValue);
    _liteHtmlWidget->horizontalScrollBar()->setValue(horizontalScrollBarValue);
}

void QLiteHtmlSearchWidget::deactivate()
{
    hide();
    _liteHtmlWidget->setFocus();
}

bool QLiteHtmlSearchWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            deactivate();
            return true;
        } else if ((keyEvent->modifiers().testFlag(Qt::ShiftModifier)
                    && (keyEvent->key() == Qt::Key_Return))
                   || (keyEvent->key() == Qt::Key_Up)) {
            doSearchUp();
            return true;
        } else if ((keyEvent->key() == Qt::Key_Return) || (keyEvent->key() == Qt::Key_Down)) {
            doSearchDown();
            return true;
        } else if (keyEvent->key() == Qt::Key_F3) {
            doSearch(!keyEvent->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }

        return false;
    }

    return QWidget::eventFilter(obj, event);
}

void QLiteHtmlSearchWidget::searchLineEditTextChanged(const QString &arg1)
{
    // If the search term is too short, just clear the style without jumping to
    // the top of the document
    if (!shouldStartSearch(arg1)) {
        _debounceTimer.stop();
        m_searchLineEdit->setStyleSheet(QString());
        return;
    }

    // Debounce: delay the search while the user is still typing
    _debounceTimer.start();
}

void QLiteHtmlSearchWidget::doSearchUp()
{
    doSearch(false);
}

void QLiteHtmlSearchWidget::doSearchDown()
{
    doSearch(true);
}

/**
 * @brief Searches for text in the LiteHtml widget
 * @returns true if found
 */
bool QLiteHtmlSearchWidget::doSearch(bool searchDown, bool allowRestartAtTop)
{
    QString text = m_searchLineEdit->text();

    if (!shouldStartSearch(text)) {
        m_searchLineEdit->setStyleSheet(QString());
        return false;
    }

    int searchMode = m_modeComboBox->currentIndex();

    QFlags<QTextDocument::FindFlag> options = searchDown ? QTextDocument::FindFlag(0)
                                                         : QTextDocument::FindBackward;
    if (searchMode == WholeWordsMode) {
        options |= QTextDocument::FindWholeWords;
    }

    if (m_matchCaseSensitiveButton->isChecked()) {
        options |= QTextDocument::FindCaseSensitively;
    }

    bool wrapped = false;
    // Use incremental=true when searching from a preset selection so that the
    // search starts at the beginning of the selection, keeping the first result
    // at the originally selected word (for #3541)
    const bool incremental = _searchFromSelection;
    _searchFromSelection = false;
    bool found = _liteHtmlWidget->findText(text, options, incremental, &wrapped);

    if (!found && allowRestartAtTop) {
        _liteHtmlWidget->findText(text, options, false, &wrapped);
    }

    // Add background and foreground colors according to whether we found the text or not
    const QString bgColorCode = _darkMode ? (found ? QStringLiteral("#135a13")
                                                   : QStringLiteral("#8d2b36"))
                                : found   ? QStringLiteral("#D5FAE2")
                                          : QStringLiteral("#FAE9EB");
    const QString fgColorCode = _darkMode ? QStringLiteral("#cccccc") : QStringLiteral("#404040");

    m_searchLineEdit->setStyleSheet(QStringLiteral("* { background: ") + bgColorCode
                                    + QStringLiteral("; color: ") + fgColorCode
                                    + QStringLiteral("; }"));

    return found;
}

void QLiteHtmlSearchWidget::setDarkMode(bool enabled)
{
    _darkMode = enabled;

    // Apply dark mode styling to buttons to ensure good contrast
    if (_darkMode) {
        // Set button icon colors to light color for visibility in dark mode
        QString buttonStyle = QStringLiteral("QPushButton { color: #cccccc; }");
        m_matchCaseSensitiveButton->setStyleSheet(buttonStyle);
    } else {
        // Reset to default styling in light mode
        m_matchCaseSensitiveButton->setStyleSheet(QString());
    }
}
