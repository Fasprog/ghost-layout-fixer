#include <src/core/BackupService.h>

#include <src/platform/SystemCommandRunner.h>
#include <src/platform/RegistryBranches.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
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

constexpr unsigned char kUtf16BomFirst = 0xFF;
constexpr unsigned char kUtf16BomSecond = 0xFE;

bool startsWithUtf16LeBom(const std::string& content)
{
    return content.size() >= 2 &&
           static_cast<unsigned char>(content[0]) == kUtf16BomFirst &&
           static_cast<unsigned char>(content[1]) == kUtf16BomSecond;
}

std::string stripRegHeaderFromUtf16Le(const std::string& content)
{
    static const std::string kUtf16HeaderTerminator(
        "\x0D\x00\x0A\x00\x0D\x00\x0A\x00",
        8);

    std::size_t contentStart = startsWithUtf16LeBom(content) ? 2 : 0;
    const std::size_t headerEnd = content.find(kUtf16HeaderTerminator, contentStart);
    if (headerEnd == std::string::npos)
    {
        return content.substr(contentStart);
    }

    contentStart = headerEnd + kUtf16HeaderTerminator.size();
    return content.substr(contentStart);
}

std::string stripRegHeaderFromAscii(const std::string& content)
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

std::string stripRegHeader(const std::string& content)
{
    if (startsWithUtf16LeBom(content))
    {
        return stripRegHeaderFromUtf16Le(content);
    }

    return stripRegHeaderFromAscii(content);
}

void writeUtf16Le(std::ofstream& stream, const std::u16string& text)
{
    stream.write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.size() * sizeof(char16_t)));
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
    std::vector<wchar_t> buffer(512, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }

        if (length + 1 < buffer.size())
        {
            buffer.resize(length);
            return std::filesystem::path(buffer.data()).parent_path();
        }

        if (buffer.size() >= 32768)
        {
            return {};
        }

        buffer.resize(buffer.size() * 2, L'\0');
    }
}

bool hasRequiredRegHeader(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }

    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty())
    {
        return false;
    }

    static const std::string kHeader = "Windows Registry Editor Version 5.00";
    static const std::string kUtf16Header =
        "\x57\x00\x69\x00\x6E\x00\x64\x00\x6F\x00\x77\x00\x73\x00\x20\x00\x52\x00\x65\x00\x67\x00\x69\x00\x73\x00\x74\x00\x72\x00\x79\x00\x20\x00\x45\x00\x64\x00\x69\x00\x74\x00\x6F\x00\x72\x00\x20\x00\x56\x00\x65\x00\x72\x00\x73\x00\x69\x00\x6F\x00\x6E\x00\x20\x00\x35\x00\x2E\x00\x30\x00\x30\x00";

    std::size_t offset = 0;
    if (startsWithUtf16LeBom(bytes))
    {
        offset = 2;
    }
    else if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
             static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF)
    {
        offset = 3;
    }

    if (bytes.size() >= offset + kHeader.size() && bytes.compare(offset, kHeader.size(), kHeader) == 0)
    {
        return true;
    }

    if (bytes.size() >= offset + kUtf16Header.size() && bytes.compare(offset, kUtf16Header.size(), kUtf16Header) == 0)
    {
        return true;
    }

    return false;
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

    std::filesystem::path baseDirectory = executableDirectory();
    if (baseDirectory.empty())
    {
        baseDirectory = std::filesystem::current_path();
    }

    const std::filesystem::path backupDirectory = baseDirectory / "backups";
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

    std::ofstream mergedBackup(backupPath, std::ios::binary | std::ios::trunc);
    if (!mergedBackup)
    {
        report.errors.push_back("backup failed: cannot create file " + backupPath);
        report.success = false;
        cleanupExportedFiles();
        return report;
    }

    static const std::u16string kMergedHeader = u"\uFEFFWindows Registry Editor Version 5.00\r\n\r\n";
    writeUtf16Le(mergedBackup, kMergedHeader);
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
        std::string stripped = stripRegHeader(buffer.str());
        if (!stripped.empty())
        {
            mergedBackup.write(stripped.data(), static_cast<std::streamsize>(stripped.size()));
        }

        static const std::string kUtf16CrLf("\x0D\x00\x0A\x00", 4);
        mergedBackup.write(kUtf16CrLf.data(), static_cast<std::streamsize>(kUtf16CrLf.size()));
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

    if (!hasRequiredRegHeader(std::filesystem::path(backupPath)))
    {
        report.errors.push_back("backup file is invalid: missing Windows Registry Editor Version 5.00 header");
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
