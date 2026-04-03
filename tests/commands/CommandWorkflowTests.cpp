#include <src/cli/CliParser.h>
#include <src/core/BackupService.h>
#include <src/core/LayoutFixService.h>

#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace
{

bool expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool testScanCommandParsing()
{
    const ghost::cli::CliParser parser;
    const char* argv[] = {"ghost-layout-fixer", "scan"};
    const ghost::cli::CliOptions options = parser.parse(2, argv);

    bool ok = true;
    ok = expect(options.command == ghost::cli::CommandType::Scan, "scan command is parsed") && ok;
    ok = expect(options.parseErrors.empty(), "scan command has no parse errors") && ok;
    return ok;
}

bool testBackupCommandParsing()
{
    const ghost::cli::CliParser parser;
    const char* argv[] = {"ghost-layout-fixer", "backup"};
    const ghost::cli::CliOptions options = parser.parse(2, argv);

    bool ok = true;
    ok = expect(options.command == ghost::cli::CommandType::Backup, "backup command is parsed") && ok;
    ok = expect(options.parseErrors.empty(), "backup command has no parse errors") && ok;
    return ok;
}

bool testFixCommandParsing()
{
    const ghost::cli::CliParser parser;
    const char* argv[] = {"ghost-layout-fixer", "fix", "--layout", "en-GB", "--dry-run"};
    const ghost::cli::CliOptions options = parser.parse(5, argv);

    bool ok = true;
    ok = expect(options.command == ghost::cli::CommandType::Fix, "fix command is parsed") && ok;
    ok = expect(options.layoutCode.has_value(), "fix layout value is parsed") && ok;
    ok = expect(options.layoutCode.value_or("") == "en-GB", "fix layout value matches") && ok;
    ok = expect(options.dryRun, "fix dry-run flag is parsed") && ok;
    ok = expect(options.parseErrors.empty(), "fix command has no parse errors") && ok;
    return ok;
}

bool testRestoreCommandParsing()
{
    const ghost::cli::CliParser parser;
    const char* argv[] = {"ghost-layout-fixer", "restore", "--file", "snapshot.reg"};
    const ghost::cli::CliOptions options = parser.parse(4, argv);

    bool ok = true;
    ok = expect(options.command == ghost::cli::CommandType::Restore, "restore command is parsed") && ok;
    ok = expect(options.restoreFile.has_value(), "restore file is parsed") && ok;
    ok = expect(options.restoreFile.value_or("") == "snapshot.reg", "restore file value matches") && ok;
    ok = expect(options.parseErrors.empty(), "restore command has no parse errors") && ok;
    return ok;
}

bool testBackupPathFormat()
{
    const ghost::core::BackupService backupService;
    const std::string path = backupService.makeBackupPath();
    const std::regex pattern("^ghost-layout-backup-[0-9]{8}-[0-9]{6}\\.reg$");

    return expect(std::regex_match(path, pattern), "backup path format matches expected timestamped name");
}

bool testFixDryRunPlanContainsSteps()
{
    const ghost::core::LayoutFixService layoutFixService;
    const std::vector<std::string> matches = {
        "HKCU\\\\Keyboard Layout\\\\Preload -> 1 = 00000809"
    };

    const ghost::core::FixPlan plan =
        layoutFixService.buildDryRunPlan("en-GB", matches, "ghost-layout-backup-20260101-120000.reg");

    bool ok = true;
    ok = expect(!plan.plannedSteps.empty(), "fix dry-run plan contains steps") && ok;
    ok = expect(
             plan.plannedSteps.front() == "validate privileges and input arguments",
             "fix dry-run plan starts with validation step") &&
        ok;

    bool containsCleanupStep = false;
    for (const std::string& step : plan.plannedSteps)
    {
        if (step.find("remove registry value:") != std::string::npos)
        {
            containsCleanupStep = true;
            break;
        }
    }
    ok = expect(containsCleanupStep, "fix dry-run plan includes registry cleanup step") && ok;
    return ok;
}

bool testRestoreBackupFailsWhenFileMissing()
{
    const ghost::core::BackupService backupService;
    const std::filesystem::path definitelyMissingPath =
        std::filesystem::temp_directory_path() / "ghost-layout-fixer-missing-backup.reg";
    const ghost::core::RestoreReport report = backupService.restoreBackup(definitelyMissingPath.string());

    bool ok = true;
    ok = expect(!report.success, "restore should fail when backup file is missing") && ok;
    ok = expect(report.executedCommands.empty(), "restore should not run commands for missing file") && ok;
    ok = expect(!report.errors.empty(), "restore should report missing-file error") && ok;
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok = testScanCommandParsing() && ok;
    ok = testBackupCommandParsing() && ok;
    ok = testFixCommandParsing() && ok;
    ok = testRestoreCommandParsing() && ok;
    ok = testBackupPathFormat() && ok;
    ok = testFixDryRunPlanContainsSteps() && ok;
    ok = testRestoreBackupFailsWhenFileMissing() && ok;

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] Command workflow tests completed\n";
    return 0;
}
