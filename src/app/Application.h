#pragma once

#include <memory>
#include <QtWidgets/QApplication>
#include "app/ApplicationContext.h"
#include "ui/screen/main/MainWindow.h"

namespace app {
    /**
     * @brief 应用程序宿主类，负责全局运行时环境、资源与子系统的引导初始化及主循环管理
     */
    class Application {
    public:
        Application(int &argc, char **argv);
        ~Application();

        /**
         * @brief 启动主事件循环并展示主窗口
         * @return 应用程序退出代码
         */
        int run();

        ApplicationContext &context() { return m_context; }
        const ApplicationContext &context() const { return m_context; }

    private:
        void initializeResources();
        void initializeSettings();
        void initializeDatabase();

        std::unique_ptr<QApplication> m_qapp;
        ApplicationContext m_context;
        std::unique_ptr<ui::screen::main::MainWindow> m_mainWindow;
    };
} // namespace app
