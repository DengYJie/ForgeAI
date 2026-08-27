#pragma once

#include "application/ports/IShellService.h"
#include <mutex>

namespace services::process {

    /**
     * @brief 终端 Shell 服务实现
     */
    class ShellService final : public application::ports::IShellService {
    public:
        ShellService();
        ~ShellService() override = default;

        QList<domain::process::ShellProfile> availableShells() const override;
        std::optional<domain::process::ShellProfile> shell(const QString& id) const override;
        std::optional<domain::process::ShellProfile> defaultShell() const override;
        bool setDefaultShell(const QString& id) override;
        void refreshAvailableShells() override;

    private:
        void detectSystemShells();

        mutable std::mutex m_mutex;
        QList<domain::process::ShellProfile> m_shells;
        QString m_defaultShellId;
    };

} // namespace services::process
