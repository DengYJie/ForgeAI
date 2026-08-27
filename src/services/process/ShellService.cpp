#include "ShellService.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QDir>

namespace services::process {

    ShellService::ShellService() {
        detectSystemShells();
    }

    QList<domain::process::ShellProfile> ShellService::availableShells() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_shells;
    }

    std::optional<domain::process::ShellProfile> ShellService::shell(const QString& id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& s : m_shells) {
            if (s.id == id) {
                return s;
            }
        }
        return std::nullopt;
    }

    std::optional<domain::process::ShellProfile> ShellService::defaultShell() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_defaultShellId.isEmpty()) {
            for (const auto& s : m_shells) {
                if (s.id == m_defaultShellId && s.enabled) {
                    return s;
                }
            }
        }
        if (!m_shells.isEmpty()) {
            return m_shells.first();
        }
        return std::nullopt;
    }

    bool ShellService::setDefaultShell(const QString& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& s : m_shells) {
            if (s.id == id) {
                m_defaultShellId = id;
                return true;
            }
        }
        return false;
    }

    void ShellService::refreshAvailableShells() {
        detectSystemShells();
    }

    void ShellService::detectSystemShells() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shells.clear();

#ifdef Q_OS_WIN
        // 1. PowerShell 7 (pwsh.exe)
        QString pwshPath = QStandardPaths::findExecutable(QStringLiteral("pwsh.exe"));
        if (pwshPath.isEmpty()) {
            const QStringList pwshCandidates = {
                QStringLiteral("C:/Program Files/PowerShell/7/pwsh.exe"),
                QStringLiteral("C:/Program Files/PowerShell/7-preview/pwsh.exe"),
                QDir::homePath() + QStringLiteral("/AppData/Local/Microsoft/WindowsApps/pwsh.exe")
            };
            for (const auto& c : pwshCandidates) {
                if (QFileInfo::exists(c)) {
                    pwshPath = c;
                    break;
                }
            }
        }
        if (!pwshPath.isEmpty()) {
            domain::process::ShellProfile pwsh;
            pwsh.id = QStringLiteral("powershell-7");
            pwsh.name = QStringLiteral("PowerShell 7");
            pwsh.type = domain::process::ShellType::PowerShell;
            pwsh.executable = pwshPath;
            pwsh.startupArguments = {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive")};
            pwsh.commandArgument = QStringLiteral("-Command");
            pwsh.enabled = true;
            m_shells.append(pwsh);
        }

        // 2. Windows PowerShell (powershell.exe)
        QString psPath = QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
        if (psPath.isEmpty()) {
            const QString defaultPs = QStringLiteral("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe");
            if (QFileInfo::exists(defaultPs)) {
                psPath = defaultPs;
            }
        }
        if (!psPath.isEmpty()) {
            domain::process::ShellProfile ps;
            ps.id = QStringLiteral("windows-powershell");
            ps.name = QStringLiteral("Windows PowerShell");
            ps.type = domain::process::ShellType::PowerShell;
            ps.executable = psPath;
            ps.startupArguments = {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass")};
            ps.commandArgument = QStringLiteral("-Command");
            ps.enabled = true;
            m_shells.append(ps);
        }

        // 3. Command Prompt (cmd.exe)
        QString cmdPath = QStandardPaths::findExecutable(QStringLiteral("cmd.exe"));
        if (cmdPath.isEmpty()) {
            const QString defaultCmd = QStringLiteral("C:/Windows/System32/cmd.exe");
            if (QFileInfo::exists(defaultCmd)) {
                cmdPath = defaultCmd;
            }
        }
        if (!cmdPath.isEmpty()) {
            domain::process::ShellProfile cmd;
            cmd.id = QStringLiteral("cmd");
            cmd.name = QStringLiteral("Command Prompt");
            cmd.type = domain::process::ShellType::Cmd;
            cmd.executable = cmdPath;
            cmd.startupArguments = {QStringLiteral("/D"), QStringLiteral("/S")};
            cmd.commandArgument = QStringLiteral("/C");
            cmd.enabled = true;
            m_shells.append(cmd);
        }

        // 4. Git Bash (bash.exe)
        QString gitBashPath = QStandardPaths::findExecutable(QStringLiteral("bash.exe"));
        if (gitBashPath.isEmpty()) {
            const QStringList gitCandidates = {
                QStringLiteral("C:/Program Files/Git/bin/bash.exe"),
                QStringLiteral("C:/Program Files/Git/usr/bin/bash.exe"),
                QDir::homePath() + QStringLiteral("/AppData/Local/Programs/Git/bin/bash.exe")
            };
            for (const auto& c : gitCandidates) {
                if (QFileInfo::exists(c)) {
                    gitBashPath = c;
                    break;
                }
            }
        }
        if (!gitBashPath.isEmpty()) {
            domain::process::ShellProfile gitBash;
            gitBash.id = QStringLiteral("git-bash");
            gitBash.name = QStringLiteral("Git Bash");
            gitBash.type = domain::process::ShellType::Bash;
            gitBash.executable = gitBashPath;
            gitBash.startupArguments = {QStringLiteral("-l")};
            gitBash.commandArgument = QStringLiteral("-c");
            gitBash.enabled = true;
            m_shells.append(gitBash);
        }

        // 5. WSL (wsl.exe)
        QString wslPath = QStandardPaths::findExecutable(QStringLiteral("wsl.exe"));
        if (!wslPath.isEmpty()) {
            domain::process::ShellProfile wsl;
            wsl.id = QStringLiteral("wsl");
            wsl.name = QStringLiteral("WSL");
            wsl.type = domain::process::ShellType::Wsl;
            wsl.executable = wslPath;
            wsl.startupArguments = {};
            wsl.commandArgument = QStringLiteral("-e");
            wsl.enabled = true;
            m_shells.append(wsl);
        }

#else
        // Linux / macOS Shell Detection
        const QString envShell = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SHELL"));
        if (!envShell.isEmpty() && QFileInfo::exists(envShell)) {
            domain::process::ShellProfile defaultEnv;
            defaultEnv.id = QStringLiteral("default-shell");
            defaultEnv.name = QFileInfo(envShell).fileName();
            defaultEnv.executable = envShell;
            defaultEnv.startupArguments = {QStringLiteral("-l")};
            defaultEnv.commandArgument = QStringLiteral("-c");
            if (defaultEnv.name == QStringLiteral("zsh")) {
                defaultEnv.type = domain::process::ShellType::Zsh;
            } else if (defaultEnv.name == QStringLiteral("fish")) {
                defaultEnv.type = domain::process::ShellType::Fish;
            } else {
                defaultEnv.type = domain::process::ShellType::Bash;
            }
            defaultEnv.enabled = true;
            m_shells.append(defaultEnv);
        }

        // bash
        const QString bashPath = QStandardPaths::findExecutable(QStringLiteral("bash"));
        if (!bashPath.isEmpty() && (m_shells.isEmpty() || m_shells.first().executable != bashPath)) {
            domain::process::ShellProfile bash;
            bash.id = QStringLiteral("bash");
            bash.name = QStringLiteral("Bash");
            bash.type = domain::process::ShellType::Bash;
            bash.executable = bashPath;
            bash.startupArguments = {QStringLiteral("-l")};
            bash.commandArgument = QStringLiteral("-c");
            bash.enabled = true;
            m_shells.append(bash);
        }

        // zsh
        const QString zshPath = QStandardPaths::findExecutable(QStringLiteral("zsh"));
        if (!zshPath.isEmpty() && (m_shells.isEmpty() || m_shells.first().executable != zshPath)) {
            domain::process::ShellProfile zsh;
            zsh.id = QStringLiteral("zsh");
            zsh.name = QStringLiteral("Zsh");
            zsh.type = domain::process::ShellType::Zsh;
            zsh.executable = zshPath;
            zsh.startupArguments = {QStringLiteral("-l")};
            zsh.commandArgument = QStringLiteral("-c");
            zsh.enabled = true;
            m_shells.append(zsh);
        }

        // sh fallback
        const QString shPath = QStandardPaths::findExecutable(QStringLiteral("sh"));
        if (!shPath.isEmpty() && m_shells.isEmpty()) {
            domain::process::ShellProfile sh;
            sh.id = QStringLiteral("sh");
            sh.name = QStringLiteral("sh");
            sh.type = domain::process::ShellType::Bash;
            sh.executable = shPath;
            sh.startupArguments = {};
            sh.commandArgument = QStringLiteral("-c");
            sh.enabled = true;
            m_shells.append(sh);
        }
#endif

        if (m_defaultShellId.isEmpty() && !m_shells.isEmpty()) {
            m_defaultShellId = m_shells.first().id;
        }
    }

} // namespace services::process
