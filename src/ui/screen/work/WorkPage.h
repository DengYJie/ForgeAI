#pragma once

#include <QWidget>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::work {
    class WorkViewModel;
    struct WorkState;

    class WorkPage : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
        Q_OBJECT

    public:
        explicit WorkPage(QWidget *parent = nullptr);

        ~WorkPage() override = default;

        void onThemeUpdated() override;

    private:
        void setupUi();

        void setupViewModel();

        void render(const WorkState &state);

        WorkViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;
        fluent::textfields::Label *m_titleLabel = nullptr;
        fluent::textfields::Label *m_subtitleLabel = nullptr;
    };
} // namespace ui::screen::work
