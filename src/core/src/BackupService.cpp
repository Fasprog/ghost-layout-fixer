#include <src/core/BackupService.h>

#include <src/platform/SystemCommandRunner.h>
#include <src/platform/RegistryBranches.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>
#include <windows.h>

namespace
{

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

bool hasRegExtensionCaseInsensitive(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    if (extension.size() != 4)
    {
        return false;
    }

    return (extension[0] == '.') &&
           (extension[1] == 'r' || extension[1] == 'R') &&
           (extension[2] == 'e' || extension[2] == 'E') &&
           (extension[3] == 'g' || extension[3] == 'G');
}

bool isSafePathForCommand(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    for (const char symbol : path)
    {
        if (symbol == '"' || symbol == '\r' || symbol == '\n')
        {
            return false;
        }
    }

    return true;
}

std::filesystem::path executableDirectory()
{
    std::wstring buffer(static_cast<std::size_t>(MAX_PATH), L'\0');
    DWORD copied = 0;
    while (true)
    {
        copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0)
        {
            return {};
        }

        if (copied < buffer.size() - 1)
        {
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    buffer.resize(copied);
    return std::filesystem::path(buffer).parent_path();
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
    gmtime_s(&utc, &nowTime);

    std::ostringstream name;
    name << "ghost-layout-backup-";
    name << std::put_time(&utc, "%Y%m%d-%H%M%S");
    name << ".reg";

    const std::filesystem::path backupDirectory = executableDirectory() / "backups";
    return (backupDirectory / name.str()).string();
}

BackupReport BackupService::createBackup(const std::string& backupPath) const
{
    BackupReport report;
    report.backupPath = backupPath;
    if (!isSafePathForCommand(backupPath))
    {
        report.errors.push_back("backup path contains unsupported characters: " + backupPath);
        report.success = false;
        return report;
    }

    std::vector<std::filesystem::path> exportedFiles;
    const auto cleanupExportedFiles = [&exportedFiles]()
    {
        for (const std::filesystem::path& exportedFile : exportedFiles)
        {
            std::error_code ignore;
            std::filesystem::remove(exportedFile, ignore);
        }
    };

    int branchIndex = 0;
    for (const std::string& branch : ghost::platform::kRegistryBranches)
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
        cleanupExportedFiles();
        return report;
    }

    const std::filesystem::path backupFilePath(backupPath);
    const std::filesystem::path backupDirectory = backupFilePath.parent_path();
    if (!backupDirectory.empty())
    {
        std::error_code createDirectoryError;
        std::filesystem::create_directories(backupDirectory, createDirectoryError);
        if (createDirectoryError)
        {
            report.errors.push_back("backup failed: cannot create directory " + backupDirectory.string());
            report.success = false;
            cleanupExportedFiles();
            return report;
        }
    }

    std::ofstream mergedBackup(backupPath, std::ios::binary | std::ios::trunc);
    if (!mergedBackup)
    {
        report.errors.push_back("backup failed: cannot create file " + backupPath);
        report.success = false;
        cleanupExportedFiles();
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

    cleanupExportedFiles();

    if (!mergedBackup.good())
    {
        report.errors.push_back("backup failed: write error for " + backupPath);
    }

    if (!report.errors.empty())
    {
        std::error_code ignore;
        std::filesystem::remove(backupPath, ignore);
    }

    report.success = report.errors.empty();
    return report;
}

RestoreReport BackupService::restoreBackup(const std::string& backupPath) const
{
    RestoreReport report;
    report.sourcePath = backupPath;
    if (!isSafePathForCommand(backupPath))
    {
        report.errors.push_back("backup path contains unsupported characters: " + backupPath);
        return report;
    }

    if (!std::filesystem::exists(backupPath))
    {
        report.errors.push_back("backup file does not exist: " + backupPath);
        return report;
    }

    if (!hasRegExtensionCaseInsensitive(std::filesystem::path(backupPath)))
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
