#pragma once

#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::work {
    class WorkViewModel;
    struct WorkState;

    class WorkPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit WorkPage(QWidget *parent = nullptr);

        ~WorkPage() override = default;

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
