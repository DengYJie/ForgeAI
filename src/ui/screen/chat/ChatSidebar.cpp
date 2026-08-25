#include "ChatSidebar.h"
#include "ChatSessionListModel.h"
#include "ChatSessionDelegate.h"
#include "ui/widget/basic/LeftAlignedButton.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QItemSelectionModel>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>

namespace {


    class TransparentListView : public fluent::collections::ListView {
    public:
        using fluent::collections::ListView::ListView;

    protected:
        void paintEvent(QPaintEvent *event) override {
            // 直接由 QListView 绘制委托项，不填充任何底板与圆角遮罩，使父级背景自然透出
            QListView::paintEvent(event);
        }
    };
} // namespace

namespace ui::screen::chat {
    ChatSidebar::ChatSidebar(QWidget *parent)
        : QWidget(parent) {
        setupUi();
    }

    void ChatSidebar::setupUi() {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 10, 8, 10);
        mainLayout->setSpacing(6);

        // 1. 顶部操作栏（精细紧凑 Fluent Button：新对话 + 过滤）
        m_headerWidget = new QWidget(this);
        auto *headerLayout = new QHBoxLayout(m_headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);

        m_newChatButton = new ui::widget::basic::LeftAlignedButton(m_headerWidget);
        m_newChatButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_newChatButton->setFluentLayout(fluent::basicinput::Button::IconBefore);
        m_newChatButton->setFluentSize(fluent::basicinput::Button::Small);
        m_newChatButton->setIconGlyph(Typography::Icons::Edit, 13); // 13px 光学图标

        QFont btnFont = themeFont(Typography::FontRole::Body).toQFont();
        btnFont.setPixelSize(Typography::FontSize::Caption); // 12px 精致字号
        m_newChatButton->setFont(btnFont);
        m_newChatButton->setText(tr("新对话"));
        m_newChatButton->setCursor(Qt::PointingHandCursor);
        m_newChatButton->setFixedHeight(32);
        connect(m_newChatButton, &QPushButton::clicked, this, &ChatSidebar::newChatRequested);
        headerLayout->addWidget(m_newChatButton, 1);

        m_filterButton = new fluent::basicinput::Button(m_headerWidget);
        m_filterButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_filterButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_filterButton->setFluentSize(fluent::basicinput::Button::Small);
        m_filterButton->setIconGlyph(Typography::Icons::Filter, 13); // 13px 光学图标
        m_filterButton->setFixedSize(32, 32);
        m_filterButton->setToolTip(tr("筛选与排序"));
        m_filterButton->setCursor(Qt::PointingHandCursor);
        connect(m_filterButton, &QPushButton::clicked, this, &ChatSidebar::filterRequested);
        headerLayout->addWidget(m_filterButton, 0);

        mainLayout->addWidget(m_headerWidget);

        // 2. 会话列表（原生 Fluent ListView + Delegate）
        m_listView = new TransparentListView(this);
        m_listView->setFrameShape(QFrame::NoFrame);
        m_listView->setSelectionIndicatorVisible(false);
        m_listView->setBorderVisible(false);
        m_listView->setBackgroundVisible(false);
        m_listView->setAutoFillBackground(false);
        m_listView->setSelectionMode(fluent::collections::SelectionMode::Single);
        m_listView->setPlaceholderText(tr("暂无历史会话"));

        m_model = new ChatSessionListModel(this);
        m_delegate = new ChatSessionDelegate(m_listView);
        m_listView->setModel(m_model);
        m_listView->setItemDelegate(m_delegate);

        connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
                this, [this](const QModelIndex &current, const QModelIndex &previous) {
                    Q_UNUSED(previous);
                    if (current.isValid()) {
                        const QString id = current.data(ChatSessionListModel::IdRole).toString();
                        emit sessionSelected(id);
                    }
                });

        connect(m_delegate, &ChatSessionDelegate::pinClicked,
                this, [this](const QString &id, bool pinned) {
                    setSessionPinned(id, pinned);
                    emit sessionPinToggled(id, pinned);
                });

        connect(m_delegate, &ChatSessionDelegate::archiveClicked,
                this, [this](const QString& id) { emit sessionArchiveRequested(id); });

        mainLayout->addWidget(m_listView, 1);
        // 会话列表数据由 ChatViewModel 通过 render() 分发，无需在此硬编码
    }

    void ChatSidebar::setSessions(const QList<ChatSessionItemData> &sessions, const QString &currentId) {
        if (!m_model) return;

        // 断开 currentChanged 信号防止 ViewModel 重入
        m_listView->selectionModel()->blockSignals(true);
        m_model->setSessions(sessions);
        m_listView->selectionModel()->blockSignals(false);

        // 还原选中
        if (!currentId.isEmpty()) {
            const int row = m_model->indexOf(currentId);
            if (row >= 0) {
                m_listView->setCurrentIndex(m_model->index(row, 0));
            }
        }
    }

    void ChatSidebar::addSession(const QString &id, const QString &title, bool isPinned) {
        if (!m_model)
            return;

        ChatSessionItemData item;
        item.id = id;
        item.title = title;
        item.isPinned = isPinned;
        item.timestamp = QDateTime::currentMSecsSinceEpoch();

        m_model->addSession(item);
    }

    void ChatSidebar::selectSession(const QString &id) {
        if (!m_model || !m_listView)
            return;

        const int row = m_model->indexOf(id);
        if (row >= 0) {
            const QModelIndex modelIdx = m_model->index(row, 0);
            m_listView->setCurrentIndex(modelIdx);
        }
    }

    void ChatSidebar::removeSession(const QString &id) {
        if (!m_model)
            return;

        const QString currentId = currentSelectedSessionId();
        m_model->removeSession(id);

        if (currentId == id && m_model->count() > 0) {
            selectSession(m_model->idAt(0));
        }
    }

    void ChatSidebar::setSessionPinned(const QString &id, bool pinned) {
        if (!m_model)
            return;

        m_model->setSessionPinned(id, pinned);
    }

    void ChatSidebar::setSessionTitle(const QString &id, const QString &title) {
        if (!m_model)
            return;

        m_model->setSessionTitle(id, title);
    }

    void ChatSidebar::clearSessions() {
        if (!m_model)
            return;

        m_model->clearSessions();
    }

    QString ChatSidebar::currentSelectedSessionId() const {
        if (!m_listView || !m_listView->currentIndex().isValid())
            return {};

        return m_listView->currentIndex().data(ChatSessionListModel::IdRole).toString();
    }

    void ChatSidebar::onThemeUpdated() {
        update();
    }
} // namespace ui::screen::chat
