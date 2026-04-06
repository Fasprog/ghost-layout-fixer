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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

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
                out << "Windows Registry Editor Version 5.00\r\n\r\n";
                out << "[" << branch << "]\r\n";
                out << "\"1\"=\"00000409\"\r\n";
            }

            return ghost::platform::CommandResult{rule.exitCode, rule.output};
        }

        return ghost::platform::CommandResult{};
    }
};

bool expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool testBackupPathFormat()
{
    const ghost::core::BackupService backupService;
    const std::string path = backupService.makeBackupPath();
    const std::regex pattern("^ghost-layout-backup-[0-9]{8}-[0-9]{6}\\.reg$");
    return expect(std::regex_match(path, pattern), "backup path format matches expected timestamped name");
}

bool testBackupAggregatesAllBranches()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"reg export", 0, "ok"});
    const ghost::core::BackupService backupService(&runner);

    const std::filesystem::path backupPath = std::filesystem::temp_directory_path() / "ghost-layout-fixer-backup.reg";
    const ghost::core::BackupReport report = backupService.createBackup(backupPath.string());

    std::ifstream in(backupPath, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    bool ok = true;
    ok = expect(report.success, "backup succeeds when all exports succeed") && ok;
    ok = expect(runner.commands.size() == 5, "backup exports all required registry branches") && ok;
    ok = expect(content.find("Windows Registry Editor Version 5.00") != std::string::npos, "merged backup contains registry header") && ok;
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
    std::filesystem::remove(backupPath);
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

    bool ok = true;
    ok = expect(!missingReport.success, "restore fails on missing backup") && ok;
    ok = expect(!invalidReport.success, "restore fails on invalid backup extension") && ok;
    ok = expect(!failedImportReport.success, "restore reports reg import failure") && ok;

    std::filesystem::remove(invalid);
    std::filesystem::remove(existing);
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

    const ghost::platform::RegistryService registryService(&runner);
    const ghost::core::LayoutFixService layoutFixService(&runner);
    const std::vector<ghost::platform::RegistryMatch> matches = {{"HKCU", "HKEY_CURRENT_USER\\Keyboard Layout\\Preload", "1", "00000409"}};

    const ghost::core::FixReport fixReport =
        layoutFixService.executeFix("en-US", matches, "backup.reg", registryService);

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

bool testCliAndYesAndNoAdmin()
{
    const ghost::cli::CliParser parser;
    const char* invalidArgv[] = {"ghost-layout-fixer", "restore", "--dry-run", "--file", "a.reg"};
    const ghost::cli::CliOptions invalidOptions = parser.parse(5, invalidArgv);

    FakeCommandRunner runner;
    runner.rules.push_back({"reg export", 0, "ok"});
    runner.rules.push_back({"reg import", 0, "ok"});
    runner.rules.push_back({"GetCultures", 0, "0409=en-US\n"});
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

    ghost::cli::CliOptions fixNoYes;
    fixNoYes.command = ghost::cli::CommandType::Fix;
    fixNoYes.layoutCode = "en-US";
    const std::size_t commandsBefore = runner.commands.size();
    const int fixNoYesCode = app.run(fixNoYes);
    const std::size_t commandsAfterNoYes = runner.commands.size();

    ghost::cli::CliOptions fixYes = fixNoYes;
    fixYes.assumeYes = true;
    const int fixYesCode = app.run(fixYes);

    bool ok = true;
    ok = expect(!invalidOptions.parseErrors.empty(), "parser returns conflicts for invalid args") && ok;
    ok = expect(noAdminCode == static_cast<int>(ghost::core::ExitCode::InsufficientPrivileges), "application blocks dangerous operations without admin") && ok;
    ok = expect(fixNoYesCode == static_cast<int>(ghost::core::ExitCode::GeneralError), "fix without --yes is blocked") && ok;
    ok = expect(commandsAfterNoYes == commandsBefore, "fix without --yes does not execute system commands") && ok;
    ok = expect(fixYesCode == static_cast<int>(ghost::core::ExitCode::Success), "fix with --yes executes") && ok;
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok = testBackupPathFormat() && ok;
    ok = testBackupAggregatesAllBranches() && ok;
    ok = testBackupPartialFailure() && ok;
    ok = testRestoreBackupValidationAndFailure() && ok;
    ok = testRegistryFailures() && ok;
    ok = testLayoutFixFailurePathsAndScanCases() && ok;
    ok = testCliAndYesAndNoAdmin() && ok;

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] Command workflow tests completed\n";
    return 0;
}
