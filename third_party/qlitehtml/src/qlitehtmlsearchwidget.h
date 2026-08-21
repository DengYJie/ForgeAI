#pragma once

#include <QEvent>
#include <QTimer>
#include <QWidget>

class QLiteHtmlWidget;
class QComboBox;
class QLineEdit;
class QPushButton;

class QLiteHtmlSearchWidget : public QWidget
{
    Q_OBJECT

public:
    enum SearchMode { PlainTextMode, WholeWordsMode, RegularExpressionMode };

    explicit QLiteHtmlSearchWidget(QLiteHtmlWidget *parent = nullptr);
    bool doSearch(bool searchDown = true, bool allowRestartAtTop = true);
    void setDarkMode(bool enabled);
    ~QLiteHtmlSearchWidget();

private:
    // UI is built in code (no .ui file) — see the constructor.
    QPushButton *m_closeButton = nullptr;
    QLineEdit *m_searchLineEdit = nullptr;
    QPushButton *m_searchDownButton = nullptr;
    QPushButton *m_searchUpButton = nullptr;
    QComboBox *m_modeComboBox = nullptr;
    QPushButton *m_matchCaseSensitiveButton = nullptr;
    QTimer _debounceTimer;

protected:
    QLiteHtmlWidget *_liteHtmlWidget;
    bool _darkMode;
    // When true, the next doSearch() will use incremental=true so that the
    // search starts at the beginning of the current selection (for #3541)
    bool _searchFromSelection = false;
    bool eventFilter(QObject *obj, QEvent *event);

public slots:
    void activate();
    void deactivate();
    void doSearchDown();
    void doSearchUp();

protected slots:
    void searchLineEditTextChanged(const QString &arg1);
};
