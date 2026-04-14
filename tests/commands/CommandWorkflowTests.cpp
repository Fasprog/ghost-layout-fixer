#include <src/app/ApplicationService.h>
#include <src/cli/CliParser.h>
#include <src/core/BackupService.h>
#include <src/core/ExitCodes.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/PrivilegeService.h>
#include <src/platform/RegistryService.h>
#include <src/platform/SystemCommandRunner.h>
#include <src/report/ReportPrinter.h>
#include <src/platform/RegistryBranches.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <windows.h>

namespace
{

struct FakePrivilegeService final : ghost::platform::IPrivilegeService
{
    bool isAdmin{true};
    bool isRunningAsAdmin() const override { return isAdmin; }
};

struct FakeCommandRunner final : ghost::platform::ICommandRunner
{
    struct Rule
    {
        std::string contains;
        int exitCode{0};
        std::string output;
    };

    mutable std::vector<std::string> commands;
    std::vector<Rule> rules;

    static std::string extractQuoted(const std::string& command, int quoteIndex)
    {
        int current = -1;
        std::size_t segmentIndex = 0;
        for (std::size_t i = 0; i < command.size(); ++i)
        {
            if (command[i] == '"')
            {
                if (current == -1)
                {
                    current = static_cast<int>(i);
                    continue;
                }

                if (segmentIndex == static_cast<std::size_t>(quoteIndex))
                {
                    return command.substr(static_cast<std::size_t>(current + 1), i - static_cast<std::size_t>(current + 1));
                }
                ++segmentIndex;
                current = -1;
            }
        }

        return {};
    }

    ghost::platform::CommandResult run(const std::string& command) const override
    {
        commands.push_back(command);
        for (const Rule& rule : rules)
        {
            if (command.find(rule.contains) == std::string::npos)
            {
                continue;
            }

            if (rule.exitCode == 0 && command.find("reg export") != std::string::npos)
            {
                const std::string branch = extractQuoted(command, 0);
                const std::string file = extractQuoted(command, 1);
                std::ofstream out(file, std::ios::binary | std::ios::trunc);
                const std::u16string payload = u"\uFEFFWindows Registry Editor Version 5.00\r\n\r\n[" +
                                               std::u16string(branch.begin(), branch.end()) +
                                               u"]\r\n\"1\"=\"00000409\"\r\n";
                out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size() * sizeof(char16_t)));
            }

            return ghost::platform::CommandResult{rule.exitCode, rule.output};
        }

        return ghost::platform::CommandResult{};
    }
};

std::string decodeUtf16LeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 2 || static_cast<unsigned char>(bytes[0]) != 0xFF || static_cast<unsigned char>(bytes[1]) != 0xFE)
    {
        return {};
    }

    std::string decoded;
    decoded.reserve(bytes.size() / 2);
    for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
    {
        const unsigned char lo = static_cast<unsigned char>(bytes[i]);
        const unsigned char hi = static_cast<unsigned char>(bytes[i + 1]);
        if (hi == 0)
        {
            decoded.push_back(static_cast<char>(lo));
        }
    }

    return decoded;
}

bool expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

std::size_t countCommandsContaining(
    const std::vector<std::string>& commands,
    const std::string& token)
{
    std::size_t count = 0;
    for (const std::string& command : commands)
    {
        if (command.find(token) != std::string::npos)
        {
            ++count;
        }
    }
    return count;
}

bool testBackupPathFormat()
{
    const ghost::core::BackupService backupService;
    const std::string path = backupService.makeBackupPath();
    const std::regex pattern(R"(.*[\\/]backups[\\/]ghost-layout-backup-[0-9]{8}-[0-9]{6}\.reg$)");

    std::wstring modulePath(static_cast<std::size_t>(MAX_PATH), L'\0');
    DWORD copied = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    modulePath.resize(copied);
    const std::filesystem::path exeDirectory = std::filesystem::path(modulePath).parent_path();
    const std::filesystem::path backupPath(path);

    bool ok = true;
    ok = expect(std::regex_match(path, pattern), "backup path format matches expected timestamped name in backups directory") && ok;
    ok = expect(backupPath.is_absolute(), "backup path is absolute") && ok;
    ok = expect(backupPath.parent_path().filename() == "backups", "backup path includes backups subdirectory") && ok;
    ok = expect(backupPath.parent_path().parent_path() == exeDirectory, "backup path is located near current executable") && ok;
    return ok;
}

bool testBackupAggregatesAllBranches()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"reg export", 0, "ok"});
    const ghost::core::BackupService backupService(&runner);

    const std::filesystem::path backupPath = std::filesystem::temp_directory_path() / "ghost-layout-fixer-backup.reg";
    const ghost::core::BackupReport report = backupService.createBackup(backupPath.string());

    const std::string content = decodeUtf16LeFile(backupPath);

    bool ok = true;
    ok = expect(report.success, "backup succeeds when all exports succeed") && ok;
    ok = expect(runner.commands.size() == ghost::platform::kRegistryBranches.size(), "backup exports all configured registry branches") && ok;
    ok = expect(content.find("Windows Registry Editor Version 5.00") != std::string::npos, "merged backup contains registry header") && ok;
    ok = expect(
             content.find("Windows Registry Editor Version 5.00", content.find("Windows Registry Editor Version 5.00") + 1) ==
                 std::string::npos,
             "merged backup contains exactly one registry header") &&
        ok;
    ok = expect(content.find("HKEY_CURRENT_USER\\Keyboard Layout\\Preload") != std::string::npos, "backup contains preload branch") && ok;
    ok = expect(content.find("HKEY_CURRENT_USER\\Control Panel\\International\\User Profile") != std::string::npos, "backup contains user profile branch") && ok;

    std::filesystem::remove(backupPath);
    return ok;
}

bool testBackupPartialFailure()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Substitutes", 1, "denied"});
    runner.rules.push_back({"reg export", 0, "ok"});

    const ghost::core::BackupService backupService(&runner);
    const std::filesystem::path backupPath = std::filesystem::temp_directory_path() / "ghost-layout-fixer-partial.reg";
    const ghost::core::BackupReport report = backupService.createBackup(backupPath.string());

    bool ok = true;
    ok = expect(!report.success, "backup reports failure on partial export failure") && ok;
    ok = expect(!report.errors.empty(), "backup returns detailed export errors") && ok;
    ok = expect(!std::filesystem::exists(backupPath), "failed backup does not leave partial merged .reg on disk") && ok;
    bool hasLeftoverParts = false;
    for (int index = 0; index < 6; ++index)
    {
        const std::filesystem::path part = backupPath.string() + ".part" + std::to_string(index) + ".reg";
        hasLeftoverParts = hasLeftoverParts || std::filesystem::exists(part);
    }
    ok = expect(!hasLeftoverParts, "failed backup cleans temporary .part files") && ok;
    std::filesystem::remove(backupPath);
    return ok;
}

bool testBackupCreatesDirectoryWhenMissing()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"reg export", 0, "ok"});
    const ghost::core::BackupService backupService(&runner);

    const std::filesystem::path backupDirectory =
        std::filesystem::temp_directory_path() / "ghost-layout-fixer-tests" / "nested" / "backups";
    const std::filesystem::path backupPath = backupDirectory / "ghost-layout-fixer-create-dir.reg";
    std::error_code ignore;
    std::filesystem::remove_all(backupDirectory.parent_path().parent_path(), ignore);

    const ghost::core::BackupReport report = backupService.createBackup(backupPath.string());

    bool ok = true;
    ok = expect(report.success, "backup succeeds when target directory is missing") && ok;
    ok = expect(std::filesystem::exists(backupDirectory), "backup creates missing target directory") && ok;
    ok = expect(std::filesystem::exists(backupPath), "backup writes merged .reg file into created directory") && ok;

    std::filesystem::remove_all(backupDirectory.parent_path().parent_path(), ignore);
    return ok;
}

bool testRestoreBackupValidationAndFailure()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"reg import", 1, "import failed"});
    const ghost::core::BackupService backupService(&runner);

    const std::filesystem::path missing = std::filesystem::temp_directory_path() / "ghost-layout-fixer-missing.reg";
    const ghost::core::RestoreReport missingReport = backupService.restoreBackup(missing.string());

    const std::filesystem::path invalid = std::filesystem::temp_directory_path() / "ghost-layout-fixer-invalid.txt";
    std::ofstream(invalid) << "bad";
    const ghost::core::RestoreReport invalidReport = backupService.restoreBackup(invalid.string());

    const std::filesystem::path existing = std::filesystem::temp_directory_path() / "ghost-layout-fixer-existing.reg";
    std::ofstream(existing) << "Windows Registry Editor Version 5.00\n";
    const ghost::core::RestoreReport failedImportReport = backupService.restoreBackup(existing.string());

    const std::filesystem::path upperCaseExtension = std::filesystem::temp_directory_path() / "ghost-layout-fixer-existing.REG";
    std::ofstream(upperCaseExtension) << "Windows Registry Editor Version 5.00\n";
    const ghost::core::RestoreReport upperCaseExtensionReport = backupService.restoreBackup(upperCaseExtension.string());

    bool ok = true;
    ok = expect(!missingReport.success, "restore fails on missing backup") && ok;
    ok = expect(!invalidReport.success, "restore fails on invalid backup extension") && ok;
    ok = expect(!failedImportReport.success, "restore reports reg import failure") && ok;
    ok = expect(
             !upperCaseExtensionReport.errors.empty() &&
                 upperCaseExtensionReport.errors.front().find("invalid extension") == std::string::npos,
             "restore accepts .REG extension and reaches import stage") &&
        ok;

    std::filesystem::remove(invalid);
    std::filesystem::remove(existing);
    std::filesystem::remove(upperCaseExtension);
    return ok;
}

bool testRegistryFailures()
{
    FakeCommandRunner queryFailRunner;
    queryFailRunner.rules.push_back({"GetCultures", 0, "0409=en-US\n"});
    queryFailRunner.rules.push_back({"reg query", 1, "query failed"});
    const ghost::platform::RegistryService registryQueryFail(&queryFailRunner);

    const std::vector<std::string> layouts = registryQueryFail.listLayoutCodesFromRegistry();

    FakeCommandRunner deleteFailRunner;
    deleteFailRunner.rules.push_back({"reg delete", 1, "delete failed"});
    const ghost::platform::RegistryService registryDeleteFail(&deleteFailRunner);
    const std::vector<ghost::platform::RegistryMatch> matches = {{"HKCU", "HKEY_CURRENT_USER\\Keyboard Layout\\Preload", "1", "00000409"}};
    const std::vector<std::string> errors = registryDeleteFail.deleteMatches(matches);

    bool ok = true;
    ok = expect(layouts.empty(), "registry list is empty when reg query fails") && ok;
    ok = expect(!errors.empty(), "registry delete returns error details on failure") && ok;
    return ok;
}

bool testLayoutFixFailurePathsAndScanCases()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"powershell", 1, "powershell failed"});
    runner.rules.push_back({"reg delete", 1, "delete failed"});

    const ghost::core::LayoutFixService layoutFixService(&runner);
    const ghost::core::FixReport fixReport =
        layoutFixService.executeFix("en-US", "backup.reg");

    const ghost::core::ScanResult emptyGhostScan =
        layoutFixService.scan({"EN_us", "ru"}, {"en-US", "ru-RU"});
    const ghost::core::ScanResult ghostScan =
        layoutFixService.scan({"en-US", "de-DE"}, {"en-US"});

    bool ok = true;
    ok = expect(!fixReport.success, "fix fails when powershell/delete fail") && ok;
    ok = expect(!fixReport.errors.empty(), "fix returns failure details") && ok;
    ok = expect(emptyGhostScan.ghostLayouts.empty(), "scan handles mixed case and underscore normalization") && ok;
    ok = expect(ghostScan.ghostLayouts.size() == 1 && ghostScan.ghostLayouts.front() == "de-DE", "scan detects non-installed layout") && ok;
    return ok;
}

bool testCliAndNoAdmin()
{
    const ghost::cli::CliParser parser;
    const char* invalidArgv[] = {"ghost-layout-fixer", "restore", "--dry-run", "--file", "a.reg"};
    const ghost::cli::CliOptions invalidOptions = parser.parse(5, invalidArgv);

    FakeCommandRunner runner;
    runner.rules.push_back({"reg export", 0, "ok"});
    runner.rules.push_back({"reg import", 0, "ok"});
    runner.rules.push_back({"GetCultures", 0, "0409=en-US\n"});
    runner.rules.push_back({"$layout = 'en-US'", 0, "ok"});
    runner.rules.push_back({"$layout = 'dp-D0'", 1, "culture missing"});
    runner.rules.push_back({"$layout = 'dr-Dp'", 1, "culture missing"});
    runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000409\n"});
    runner.rules.push_back({"powershell", 0, "ok"});

    ghost::core::BackupService backupService(&runner);
    ghost::platform::RegistryService registryService(&runner);
    ghost::platform::InstalledLanguageService installedLanguageService(&runner);
    ghost::core::LayoutFixService layoutFixService(&runner);
    ghost::report::ReportPrinter printer;

    FakePrivilegeService notAdmin;
    notAdmin.isAdmin = false;
    ghost::app::ApplicationService appNoAdmin(
        notAdmin,
        backupService,
        registryService,
        installedLanguageService,
        layoutFixService,
        printer);
    ghost::cli::CliOptions backupOptions;
    backupOptions.command = ghost::cli::CommandType::Backup;
    const int noAdminCode = appNoAdmin.run(backupOptions);

    FakePrivilegeService admin;
    admin.isAdmin = true;
    ghost::app::ApplicationService app(
        admin,
        backupService,
        registryService,
        installedLanguageService,
        layoutFixService,
        printer);

    ghost::cli::CliOptions fixOptions;
    fixOptions.command = ghost::cli::CommandType::Fix;
    fixOptions.layoutCode = "en-US";

    const std::size_t commandsBefore = runner.commands.size();
    const int fixCode = app.run(fixOptions);
    const std::size_t commandsAfter = runner.commands.size();

    ghost::cli::CliOptions invalidFixOptions;
    invalidFixOptions.command = ghost::cli::CommandType::Fix;
    invalidFixOptions.layoutCode = "dp-D0";
    const std::size_t invalidFixCommandsBefore = runner.commands.size();
    const int invalidFixCode = app.run(invalidFixOptions);
    const std::size_t invalidFixCommandsAfter = runner.commands.size();

    ghost::cli::CliOptions invalidFixOptionsSecond;
    invalidFixOptionsSecond.command = ghost::cli::CommandType::Fix;
    invalidFixOptionsSecond.layoutCode = "dr-Dp";
    const std::size_t invalidFixCommandsSecondBefore = runner.commands.size();
    const int invalidFixCodeSecond = app.run(invalidFixOptionsSecond);
    const std::size_t invalidFixCommandsSecondAfter = runner.commands.size();

    ghost::cli::CliOptions invalidDryRunOptions;
    invalidDryRunOptions.command = ghost::cli::CommandType::Fix;
    invalidDryRunOptions.layoutCode = "invalid!";
    invalidDryRunOptions.dryRun = true;
    const std::size_t invalidDryRunCommandsBefore = runner.commands.size();
    const int invalidDryRunCode = app.run(invalidDryRunOptions);
    const std::size_t invalidDryRunCommandsAfter = runner.commands.size();

    bool ok = true;
    ok = expect(!invalidOptions.parseErrors.empty(), "parser returns conflicts for invalid args") && ok;
    ok = expect(noAdminCode == static_cast<int>(ghost::core::ExitCode::InsufficientPrivileges), 
                "application blocks dangerous operations without admin") && ok;
    ok = expect(fixCode == static_cast<int>(ghost::core::ExitCode::Success), "fix executes") && ok;
    ok = expect(commandsAfter > commandsBefore, "fix executes system commands") && ok;
    ok = expect(
             countCommandsContaining(runner.commands, "$layout = 'en-US'") == 1,
             "valid en-US fix runs SpecificCultures validation once before backup/fix") &&
        ok;
    ok = expect(invalidFixCode == static_cast<int>(ghost::core::ExitCode::FixError), "fix fails on invalid culture tag dp-D0") && ok;
    ok = expect(invalidFixCommandsAfter == invalidFixCommandsBefore + 1, "invalid dp-D0 fix only runs culture validation command") && ok;
    ok = expect(
             countCommandsContaining(runner.commands, "$layout = 'dp-D0'") == 1,
             "invalid dp-D0 triggers SpecificCultures validation") &&
        ok;
    ok = expect(
             countCommandsContaining(runner.commands, "Set-WinUserLanguageList") == 2,
             "invalid dp-D0 does not run add/remove fix commands") &&
        ok;
    ok = expect(
             countCommandsContaining(runner.commands, "reg export") == ghost::platform::kRegistryBranches.size(),
             "invalid dp-D0 does not create additional backup exports") &&
        ok;
    ok = expect(invalidFixCodeSecond == static_cast<int>(ghost::core::ExitCode::FixError), "fix fails on invalid culture tag dr-Dp") && ok;
    ok = expect(invalidFixCommandsSecondAfter == invalidFixCommandsSecondBefore + 1, "invalid dr-Dp fix only runs culture validation command") && ok;
    ok = expect(
             countCommandsContaining(runner.commands, "$layout = 'dr-Dp'") == 1,
             "invalid dr-Dp triggers SpecificCultures validation") &&
        ok;
    ok = expect(
             countCommandsContaining(runner.commands, "reg export") == ghost::platform::kRegistryBranches.size(),
             "invalid dr-Dp does not create additional backup exports") &&
        ok;
    ok = expect(invalidDryRunCode == static_cast<int>(ghost::core::ExitCode::FixError), "dry-run fails fast on invalid layout code") && ok;
    ok = expect(invalidDryRunCommandsAfter == invalidDryRunCommandsBefore, "invalid dry-run does not build plan through registry commands") && ok;
    ok = expect(
             countCommandsContaining(runner.commands, "GetCultures([System.Globalization.CultureTypes]::SpecificCultures)") == 3,
             "invalid! is rejected by regex and does not trigger SpecificCultures validation") &&
        ok;
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok = testBackupPathFormat() && ok;
    ok = testBackupAggregatesAllBranches() && ok;
    ok = testBackupPartialFailure() && ok;
    ok = testBackupCreatesDirectoryWhenMissing() && ok;
    ok = testRestoreBackupValidationAndFailure() && ok;
    ok = testRegistryFailures() && ok;
    ok = testLayoutFixFailurePathsAndScanCases() && ok;
    ok = testCliAndNoAdmin() && ok;

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] Command workflow tests completed\n";
    return 0;
}
