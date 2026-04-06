#include <src/core/BackupService.h>

#include <src/platform/SystemCommandRunner.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
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

const ghost::platform::ICommandRunner& defaultRunner()
{
    static const ghost::platform::SystemCommandRunner runner;
    return runner;
}

std::string stripRegHeader(const std::string& content)
{
    const std::size_t windowsHeaderEnd = content.find("\r\n\r\n");
    if (windowsHeaderEnd != std::string::npos)
    {
        return content.substr(windowsHeaderEnd + 4);
    }

    const std::size_t unixHeaderEnd = content.find("\n\n");
    if (unixHeaderEnd != std::string::npos)
    {
        return content.substr(unixHeaderEnd + 2);
    }

    return content;
}

} // namespace

namespace ghost::core
{

BackupService::BackupService(const ghost::platform::ICommandRunner* runner)
    : runner_(runner != nullptr ? runner : &defaultRunner())
{
}

std::string BackupService::makeBackupPath() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &nowTime);
#else
    gmtime_r(&nowTime, &utc);
#endif

    std::ostringstream name;
    name << "ghost-layout-backup-";
    name << std::put_time(&utc, "%Y%m%d-%H%M%S");
    name << ".reg";
    return name.str();
}

BackupReport BackupService::createBackup(const std::string& backupPath) const
{
    BackupReport report;
    report.backupPath = backupPath;

    std::vector<std::filesystem::path> exportedFiles;
    int branchIndex = 0;
    for (const std::string& branch : registryBranchesForBackup())
    {
        const std::filesystem::path branchBackupPath =
            std::filesystem::path(backupPath + ".part" + std::to_string(branchIndex) + ".reg");
        ++branchIndex;

        const std::string command =
            "reg export \"" + branch + "\" \"" + branchBackupPath.string() + "\" /y";
        report.executedCommands.push_back(command);
        const ghost::platform::CommandResult result = runner_->run(command);
        if (result.exitCode != 0)
        {
            report.errors.push_back("backup failed for " + branch + ": " + result.outputText);
            continue;
        }

        exportedFiles.push_back(branchBackupPath);
    }

    if (exportedFiles.empty())
    {
        report.errors.push_back("backup failed: no registry branches were exported");
        report.success = false;
        return report;
    }

    std::ofstream mergedBackup(backupPath, std::ios::binary | std::ios::trunc);
    if (!mergedBackup)
    {
        report.errors.push_back("backup failed: cannot create file " + backupPath);
        report.success = false;
        return report;
    }

    mergedBackup << "Windows Registry Editor Version 5.00\r\n\r\n";
    for (const std::filesystem::path& exportedFile : exportedFiles)
    {
        std::ifstream input(exportedFile, std::ios::binary);
        if (!input)
        {
            report.errors.push_back("backup failed: cannot read exported part " + exportedFile.string());
            continue;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        mergedBackup << stripRegHeader(buffer.str()) << "\r\n";
    }

    for (const std::filesystem::path& exportedFile : exportedFiles)
    {
        std::error_code ignore;
        std::filesystem::remove(exportedFile, ignore);
    }

    if (!mergedBackup.good())
    {
        report.errors.push_back("backup failed: write error for " + backupPath);
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

    if (std::filesystem::path(backupPath).extension() != ".reg")
    {
        report.errors.push_back("backup file has invalid extension: " + backupPath);
        return report;
    }

    if (std::filesystem::is_regular_file(backupPath) && std::filesystem::file_size(backupPath) == 0)
    {
        report.errors.push_back("backup file is empty: " + backupPath);
        return report;
    }

    const std::string command = "reg import \"" + backupPath + "\"";
    report.executedCommands.push_back(command);

    const ghost::platform::CommandResult result = runner_->run(command);
    if (result.exitCode != 0)
    {
        report.errors.push_back("restore failed: " + result.outputText);
        return report;
    }

    report.success = true;
    return report;
}

} // namespace ghost::core
