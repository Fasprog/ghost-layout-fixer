#include <src/core/BackupService.h>

#include <src/platform/SystemCommandRunner.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{

std::vector<std::string> registryBranchesForBackup()
{
    return {
        "HKEY_CURRENT_USER\\Keyboard Layout\\Preload",
        "HKEY_CURRENT_USER\\Keyboard Layout\\Substitutes",
        "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Preload",
        "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Substitutes",
        "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile"
    };
}

} // namespace

namespace ghost::core
{

std::string BackupService::makeBackupPath() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    const std::tm* utc = std::gmtime(&nowTime);

    std::ostringstream name;
    name << "ghost-layout-backup-";
    name << std::put_time(utc, "%Y%m%d-%H%M%S");
    name << ".reg";
    return name.str();
}

BackupReport BackupService::createBackup(const std::string& backupPath) const
{
    const ghost::platform::SystemCommandRunner runner;
    BackupReport report;
    report.backupPath = backupPath;

    for (const std::string& branch : registryBranchesForBackup())
    {
        const std::string command = "reg export \"" + branch + "\" \"" + backupPath + "\" /y";
        report.executedCommands.push_back(command);
        const ghost::platform::CommandResult result = runner.run(command);
        if (result.exitCode != 0)
        {
            report.errors.push_back("backup failed for " + branch + ": " + result.stdoutText);
        }
    }

    report.success = report.errors.empty();
    return report;
}

RestoreReport BackupService::restoreBackup(const std::string& backupPath) const
{
    RestoreReport report;
    report.sourcePath = backupPath;

    if (!std::filesystem::exists(backupPath))
    {
        report.errors.push_back("backup file does not exist: " + backupPath);
        return report;
    }

    const std::string command = "reg import \"" + backupPath + "\"";
    report.executedCommands.push_back(command);

    const ghost::platform::SystemCommandRunner runner;
    const ghost::platform::CommandResult result = runner.run(command);
    if (result.exitCode != 0)
    {
        report.errors.push_back("restore failed: " + result.stdoutText);
        return report;
    }

    report.success = true;
    return report;
}

} // namespace ghost::core
