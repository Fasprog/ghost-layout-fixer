#include <src/core/BackupService.h>
#include <src/platform/RegistryBranches.h>

#include <tests/test_support/TestFakes.h>
#include <tests/test_support/TestHelpers.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <windows.h>

using ghost::tests::FakeCommandRunner;
using ghost::tests::decodeUtf16LeFile;
using ghost::tests::expect;

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
    for (std::size_t index = 0; index < ghost::platform::kRegistryBranches.size(); ++index)
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
