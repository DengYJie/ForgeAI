#include "ui/screen/settings/SettingsPageFactory.h"

#include <FluentQt/Design.h>
#include <FluentQt/Layout.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/TextFields.h>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <functional>

namespace ui::screen::settings {

    namespace {
        /** 设置卡片最大宽度与间距规范 */
        constexpr int kMaxContentWidth = 1064;
        constexpr int kCardSpacing = 4;
        constexpr int kStackedCardWidthThreshold = 560;

        /**
         * @brief 支持视口透明穿透的 Fluent ScrollView
         * @details 透出窗口底层的 Mica / Acrylic 磨砂或系统背景底质，避免被 Canvas 纯色遮挡
         */
        class SettingsScrollView : public fluent::scrolling::ScrollView {
        public:
            explicit SettingsScrollView(QWidget *parent = nullptr)
                : fluent::scrolling::ScrollView(parent) {
                setWidgetResizable(true);
                setFrameShape(QFrame::NoFrame);
                setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
                setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
                applyTransparentViewport();
            }

            void onThemeUpdated() override {
                fluent::scrolling::ScrollView::onThemeUpdated();
                applyTransparentViewport();
            }

        private:
            void applyTransparentViewport() {
                setAutoFillBackground(false);
                setAttribute(Qt::WA_NoSystemBackground, true);
                setAttribute(Qt::WA_TranslucentBackground, true);

                if (auto *vp = viewport()) {
                    vp->setAutoFillBackground(false);
                    vp->setAttribute(Qt::WA_NoSystemBackground, true);
                    vp->setAttribute(Qt::WA_TranslucentBackground, true);
                    QPalette pal = vp->palette();
                    pal.setColor(QPalette::Window, Qt::transparent);
                    pal.setColor(QPalette::Base, Qt::transparent);
                    vp->setPalette(pal);
                }
            }
        };

        /**
         * @brief 动态计算边距以实现 1064px 水平居中的设置内容容器
         */
        class SettingsContentWidget : public QWidget {
        public:
            explicit SettingsContentWidget(QWidget *parent = nullptr) : QWidget(parent) {
                setAutoFillBackground(false);
            }

            void setContentLayout(QVBoxLayout *layout) {
                m_layout = layout;
                setLayout(layout);
                updateMargins();
            }

        protected:
            void resizeEvent(QResizeEvent *event) override {
                QWidget::resizeEvent(event);
                updateMargins();
            }

        private:
            void updateMargins() {
                if (!m_layout) return;
                const int w = width();
                // 宽屏 (w > 1064 + 72): 两侧边距分配为 (w - 1064)/2，使得 1064px 内容列精确水平居中
                // 窄屏 (w <= 1136): 使用标准 36px (超窄屏为 20px) 边距铺满视口
                const int marginH = (w > kMaxContentWidth + 72)
                                        ? (w - kMaxContentWidth) / 2
                                        : (w < 800 ? 20 : 36);
                const int marginV = (w < 800) ? 16 : 24;
                m_layout->setContentsMargins(marginH, marginV, marginH, 36);
            }

            QVBoxLayout *m_layout = nullptr;
        };

        class SettingsCardItem : public fluent::layout::Card {
        public:
            explicit SettingsCardItem(const QString &iconGlyph,
                                      const QString &title,
                                      const QString &subtitle,
                                      QWidget *trailingWidget,
                                      QWidget *parent = nullptr)
                : fluent::layout::Card(parent), m_trailing(trailingWidget) {
                setObjectName(QStringLiteral("settingsCardItem"));
                setMinimumHeight(64);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

                m_layout = new QGridLayout(this);
                m_layout->setContentsMargins(16, 12, 16, 12);
                m_layout->setHorizontalSpacing(16);
                m_layout->setVerticalSpacing(8);
                m_layout->setColumnStretch(0, 0);
                m_layout->setColumnStretch(1, 1);
                m_layout->setColumnStretch(2, 0);

                auto *iconLabel = new fluent::textfields::Label(this);
                iconLabel->setFont(QFont(Typography::FontFamily::FluentIcons, 14));
                iconLabel->setText(iconGlyph);
                iconLabel->setFixedWidth(24);
                iconLabel->setAlignment(Qt::AlignCenter);
                m_layout->addWidget(iconLabel, 0, 0, Qt::AlignVCenter);

                auto *textColumn = new QWidget(this);
                textColumn->setAutoFillBackground(false);
                auto *textLayout = new QVBoxLayout(textColumn);
                textLayout->setContentsMargins(0, 0, 0, 0);
                textLayout->setSpacing(2);

                auto *titleLabel = new fluent::textfields::Label(title, textColumn);
                titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
                titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
                textLayout->addWidget(titleLabel);

                if (!subtitle.isEmpty()) {
                    auto *subtitleLabel = new fluent::textfields::Label(subtitle, textColumn);
                    subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
                    subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
                    subtitleLabel->setObjectName(QStringLiteral("cardSubtitle"));
                    textLayout->addWidget(subtitleLabel);
                }

                m_layout->addWidget(textColumn, 0, 1, Qt::AlignVCenter);

                if (m_trailing) {
                    m_trailing->setParent(this);
                    m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
                }
            }

            QSize sizeHint() const override {
                return QSize(kMaxContentWidth, m_stacked ? 96 : 64);
            }

            QSize minimumSizeHint() const override {
                return QSize(320, m_stacked ? 96 : 64);
            }

            void setStacked(bool stacked) {
                if (m_stacked == stacked || !m_trailing) return;
                m_stacked = stacked;

                m_layout->removeWidget(m_trailing);
                if (m_stacked) {
                    m_layout->addWidget(m_trailing, 1, 1, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
                    setMinimumHeight(96);
                } else {
                    m_layout->addWidget(m_trailing, 0, 2, 1, 1, Qt::AlignRight | Qt::AlignVCenter);
                    setMinimumHeight(64);
                }
                m_trailing->show();
                updateGeometry();
            }

        protected:
            void resizeEvent(QResizeEvent *event) override {
                fluent::layout::Card::resizeEvent(event);
                updateLayoutMode();
            }

        private:
            void updateLayoutMode() {
                if (width() <= 0) return;
                setStacked(width() < kStackedCardWidthThreshold);
            }

            QGridLayout *m_layout = nullptr;
            QWidget *m_trailing = nullptr;
            bool m_stacked = false;
        };

        class LazyProviderPage : public QWidget {
        public:
            explicit LazyProviderPage(std::function<QWidget *(QWidget *)> factory, QWidget *parent = nullptr)
                : QWidget(parent), m_factory(std::move(factory)) {
                setAutoFillBackground(false);
                auto *rootLayout = new QVBoxLayout(this);
                rootLayout->setContentsMargins(0, 0, 0, 0);
            }

        protected:
            void showEvent(QShowEvent *event) override {
                QWidget::showEvent(event);
                if (m_realPage || !m_factory) return;

                m_realPage = m_factory(this);
                if (m_realPage && layout()) {
                    layout()->addWidget(m_realPage);
                }
            }

        private:
            std::function<QWidget *(QWidget *)> m_factory;
            QWidget *m_realPage = nullptr;
        };
    } // namespace

    QWidget *SettingsPageFactory::createLazyPage(
        const SettingsProviderPageDescriptor &descriptor,
        QList<QVBoxLayout *> &pageLayouts,
        QList<QWidget *> &cards,
        QWidget *parent) {

        return new LazyProviderPage(
            [descriptor, &pageLayouts, &cards](QWidget *lazyParent) {
                return descriptor.hasCustomPage()
                           ? createCustomProviderPage(descriptor, lazyParent)
                           : createGenericProviderPage(descriptor, pageLayouts, cards, lazyParent);
            },
            parent
        );
    }

    QWidget *SettingsPageFactory::createCustomProviderPage(
        const SettingsProviderPageDescriptor &descriptor,
        QWidget *parent) {
        if (!descriptor.pageFactory) return new QWidget(parent);
        return descriptor.pageFactory->createProviderPage(parent);
    }

    QWidget *SettingsPageFactory::createGenericProviderPage(
        const SettingsProviderPageDescriptor &descriptor,
        QList<QVBoxLayout *> &pageLayouts,
        QList<QWidget *> &cards,
        QWidget *parent) {

        auto *scrollView = new SettingsScrollView(parent);

        auto *contentWidget = new SettingsContentWidget(scrollView);
        auto *contentLayout = new QVBoxLayout();
        contentLayout->setContentsMargins(36, 24, 36, 36);
        contentLayout->setSpacing(kCardSpacing);
        contentWidget->setContentLayout(contentLayout);

        pageLayouts.append(contentLayout);

        auto *pageTitle = new fluent::textfields::Label(descriptor.title, contentWidget);
        pageTitle->setFluentTypography(Typography::FontRole::Title);
        pageTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        contentLayout->addWidget(pageTitle);
        contentLayout->addSpacing(12);

        QString previousCategory;
        for (const auto &factory : descriptor.factories) {
            if (!factory) continue;

            const QString currentCategory = factory->categoryDisplayName();
            // 仅在分类与大标题不同，且与上一分类不同时才渲染分区小标题，避免重复显示大标题同名小标题
            if (currentCategory != previousCategory && !currentCategory.isEmpty()) {
                if (currentCategory != descriptor.title) {
                    contentLayout->addSpacing(20);
                    contentLayout->addWidget(createSectionHeader(currentCategory, contentWidget));
                    contentLayout->addSpacing(4);
                }
                previousCategory = currentCategory;
            }

            QWidget *trailingWidget = factory->createControlWidget(contentWidget);
            QWidget *card = createSettingsCard(
                factory->iconGlyph(),
                factory->title(),
                factory->subtitle(),
                trailingWidget,
                contentWidget
            );
            cards.append(card);
            contentLayout->addWidget(card);
        }

        contentLayout->addStretch();
        scrollView->setWidget(contentWidget);
        return scrollView;
    }

    QWidget *SettingsPageFactory::createSectionHeader(const QString &title, QWidget *parent) {
        auto *header = new fluent::textfields::Label(title, parent);
        header->setFluentTypography(Typography::FontRole::BodyStrong);
        header->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        return header;
    }

    QWidget *SettingsPageFactory::createSettingsCard(
        const QString &iconGlyph,
        const QString &title,
        const QString &subtitle,
        QWidget *trailingWidget,
        QWidget *parent) {

        return new SettingsCardItem(iconGlyph, title, subtitle, trailingWidget, parent);
    }

} // namespace ui::screen::settings
