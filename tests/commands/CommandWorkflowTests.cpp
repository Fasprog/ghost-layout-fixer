#include <src/app/ApplicationService.h>
#include <src/cli/CliParser.h>
#include <src/core/BackupService.h>
#include <src/core/ExitCodes.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/PrivilegeService.h>
#include <src/platform/RegistryBranches.h>
#include <src/platform/RegistryService.h>
#include <src/report/ReportPrinter.h>

#include <tests/test_support/TestFakes.h>
#include <tests/test_support/TestHelpers.h>
#include <tests/test_support/TestSuites.h>

#include <iostream>
#include <vector>

using ghost::tests::FakeCommandRunner;
using ghost::tests::FakePrivilegeService;
using ghost::tests::commandSlice;
using ghost::tests::countCommandsContaining;
using ghost::tests::expect;

bool testFixRejectsNonGhostLayoutAndAllowsGhostLayout()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"$layout = 'ru-RU'", 0, "ok"});
    runner.rules.push_back({"$layout = 'de-DE'", 0, "ok"});
    runner.rules.push_back({"GetCultures", 0, "0419=ru-RU\n0407=de-DE\n"});
    runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 0, "ru\n"});
    runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000419\n    2    REG_SZ    00000407\n"});
    runner.rules.push_back({"reg export", 0, "ok"});
    runner.rules.push_back({"reg delete", 0, "ok"});
    runner.rules.push_back({"Set-WinUserLanguageList", 0, "ok"});

    ghost::core::BackupService backupService(&runner);
    ghost::platform::RegistryService registryService(&runner);
    ghost::platform::InstalledLanguageService installedLanguageService(&runner);
    ghost::core::LayoutFixService layoutFixService(&runner);
    ghost::report::ReportPrinter printer;
    FakePrivilegeService admin;
    ghost::app::ApplicationService app(
        admin,
        backupService,
        registryService,
        installedLanguageService,
        layoutFixService,
        printer);

    ghost::cli::CliOptions nonGhostFix;
    nonGhostFix.command = ghost::cli::CommandType::Fix;
    nonGhostFix.layoutCode = "ru-RU";
    const std::size_t nonGhostFixBefore = runner.commands.size();
    const int nonGhostFixCode = app.run(nonGhostFix);
    const std::size_t nonGhostFixAfter = runner.commands.size();

    ghost::cli::CliOptions nonGhostDryRun;
    nonGhostDryRun.command = ghost::cli::CommandType::Fix;
    nonGhostDryRun.layoutCode = "ru-RU";
    nonGhostDryRun.dryRun = true;
    const std::size_t nonGhostDryRunBefore = runner.commands.size();
    const int nonGhostDryRunCode = app.run(nonGhostDryRun);
    const std::size_t nonGhostDryRunAfter = runner.commands.size();

    bool ok = true;
    ok = expect(nonGhostFixCode == static_cast<int>(ghost::core::ExitCode::FixError), "fix rejects non-ghost ru-RU when ru is installed") && ok;
    ok = expect(nonGhostFixAfter > nonGhostFixBefore, "non-ghost fix performs validation and ghost checks") && ok;
    const std::vector<std::string> nonGhostFixCommands =
        commandSlice(runner.commands, nonGhostFixBefore, nonGhostFixAfter);
    ok = expect(
             countCommandsContaining(nonGhostFixCommands, "reg export") == 0,
             "non-ghost fix does not create backup before ghost check") &&
        ok;
    ok = expect(
             countCommandsContaining(nonGhostFixCommands, "Set-WinUserLanguageList") == 0,
             "non-ghost fix does not run add/remove cycle") &&
        ok;
    ok = expect(
             countCommandsContaining(nonGhostFixCommands, "reg delete") == 0,
             "non-ghost fix does not run registry cleanup") &&
        ok;

    ok = expect(nonGhostDryRunCode == static_cast<int>(ghost::core::ExitCode::FixError), "dry-run rejects non-ghost ru-RU when ru is installed") && ok;
    ok = expect(nonGhostDryRunAfter > nonGhostDryRunBefore, "non-ghost dry-run performs validation and ghost checks") && ok;
    const std::vector<std::string> nonGhostDryRunCommands =
        commandSlice(runner.commands, nonGhostDryRunBefore, nonGhostDryRunAfter);
    ok = expect(
             countCommandsContaining(nonGhostDryRunCommands, "reg export") == 0,
             "non-ghost dry-run does not create backup") &&
        ok;
    ok = expect(
             countCommandsContaining(nonGhostDryRunCommands, "Set-WinUserLanguageList") == 0,
             "non-ghost dry-run does not run add/remove cycle") &&
        ok;
    ok = expect(
             countCommandsContaining(nonGhostDryRunCommands, "reg delete") == 0,
             "non-ghost dry-run does not run registry cleanup") &&
        ok;

    ghost::cli::CliOptions ghostFix;
    ghostFix.command = ghost::cli::CommandType::Fix;
    ghostFix.layoutCode = "de-DE";
    const std::size_t ghostFixBefore = runner.commands.size();
    const int ghostFixCode = app.run(ghostFix);
    const std::size_t ghostFixAfter = runner.commands.size();
    const std::vector<std::string> ghostFixCommands =
        commandSlice(runner.commands, ghostFixBefore, ghostFixAfter);
    (void)ghostFixCode;
    ok = expect(
             countCommandsContaining(ghostFixCommands, "reg export") == ghost::platform::kRegistryBranches.size(),
             "ghost de-DE fix creates backup for all configured registry branches") &&
        ok;
    ok = expect(
             countCommandsContaining(ghostFixCommands, "Set-WinUserLanguageList") == 2,
             "ghost de-DE fix runs add/remove cycle") &&
        ok;
    ok = expect(
             countCommandsContaining(ghostFixCommands, "reg delete") > 0,
             "ghost de-DE fix performs registry cleanup for matched entries") &&
        ok;
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
    runner.rules.push_back({"$layout = 'de-DE'", 0, "ok"});
    runner.rules.push_back({"$layout = 'dp-D0'", 1, "culture missing"});
    runner.rules.push_back({"$layout = 'dr-Dp'", 1, "culture missing"});
    runner.rules.push_back({"GetCultures", 0, "0409=en-US\n0407=de-DE\n"});
    runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 0, "en-US\n"});
    runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000409\n    2    REG_SZ    00000407\n"});
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
    fixOptions.layoutCode = "de-DE";

    const std::size_t commandsBefore = runner.commands.size();
    const int fixCode = app.run(fixOptions);
    const std::size_t commandsAfter = runner.commands.size();
    const std::vector<std::string> fixCommands = commandSlice(runner.commands, commandsBefore, commandsAfter);

    ghost::cli::CliOptions invalidFixOptions;
    invalidFixOptions.command = ghost::cli::CommandType::Fix;
    invalidFixOptions.layoutCode = "dp-D0";
    const std::size_t invalidFixCommandsBefore = runner.commands.size();
    const int invalidFixCode = app.run(invalidFixOptions);
    const std::size_t invalidFixCommandsAfter = runner.commands.size();
    const std::vector<std::string> invalidFixCommands =
        commandSlice(runner.commands, invalidFixCommandsBefore, invalidFixCommandsAfter);

    ghost::cli::CliOptions invalidFixOptionsSecond;
    invalidFixOptionsSecond.command = ghost::cli::CommandType::Fix;
    invalidFixOptionsSecond.layoutCode = "dr-Dp";
    const std::size_t invalidFixCommandsSecondBefore = runner.commands.size();
    const int invalidFixCodeSecond = app.run(invalidFixOptionsSecond);
    const std::size_t invalidFixCommandsSecondAfter = runner.commands.size();
    const std::vector<std::string> invalidFixCommandsSecond =
        commandSlice(runner.commands, invalidFixCommandsSecondBefore, invalidFixCommandsSecondAfter);

    ghost::cli::CliOptions invalidDryRunOptions;
    invalidDryRunOptions.command = ghost::cli::CommandType::Fix;
    invalidDryRunOptions.layoutCode = "invalid!";
    invalidDryRunOptions.dryRun = true;
    const std::size_t invalidDryRunCommandsBefore = runner.commands.size();
    const int invalidDryRunCode = app.run(invalidDryRunOptions);
    const std::size_t invalidDryRunCommandsAfter = runner.commands.size();
    const std::vector<std::string> invalidDryRunCommands =
        commandSlice(runner.commands, invalidDryRunCommandsBefore, invalidDryRunCommandsAfter);

    bool ok = true;
    ok = expect(!invalidOptions.parseErrors.empty(), "parser returns conflicts for invalid args") && ok;
    ok = expect(noAdminCode == static_cast<int>(ghost::core::ExitCode::InsufficientPrivileges),
                "application blocks dangerous operations without admin") && ok;
    ok = expect(fixCode == static_cast<int>(ghost::core::ExitCode::FixError), "fix reaches ghost de-DE flow but fails when fake registry state is unchanged after delete") && ok;
    ok = expect(commandsAfter > commandsBefore, "fix executes system commands") && ok;
    ok = expect(
             countCommandsContaining(fixCommands, "$layout = 'de-DE'") == 1,
             "valid de-DE fix runs SpecificCultures validation once before backup/fix") &&
        ok;
    ok = expect(
             countCommandsContaining(fixCommands, "Set-WinUserLanguageList") == 2,
             "ghost de-DE fix runs add/remove commands before validation failures") &&
        ok;
    ok = expect(
             countCommandsContaining(fixCommands, "reg export") == ghost::platform::kRegistryBranches.size(),
             "ghost de-DE fix exports all configured branches in backup") &&
        ok;
    ok = expect(invalidFixCode == static_cast<int>(ghost::core::ExitCode::FixError), "fix fails on invalid culture tag dp-D0") && ok;
    ok = expect(invalidFixCommandsAfter == invalidFixCommandsBefore + 1, "invalid dp-D0 fix only runs culture validation command") && ok;
    ok = expect(
             countCommandsContaining(invalidFixCommands, "$layout = 'dp-D0'") == 1,
             "invalid dp-D0 triggers SpecificCultures validation") &&
        ok;
    ok = expect(
             countCommandsContaining(invalidFixCommands, "Set-WinUserLanguageList") == 0,
             "invalid dp-D0 does not run add/remove fix commands") &&
        ok;
    ok = expect(
             countCommandsContaining(invalidFixCommands, "reg export") == 0,
             "invalid dp-D0 does not create additional backup exports") &&
        ok;
    ok = expect(invalidFixCodeSecond == static_cast<int>(ghost::core::ExitCode::FixError), "fix fails on invalid culture tag dr-Dp") && ok;
    ok = expect(invalidFixCommandsSecondAfter == invalidFixCommandsSecondBefore + 1, "invalid dr-Dp fix only runs culture validation command") && ok;
    ok = expect(
             countCommandsContaining(invalidFixCommandsSecond, "$layout = 'dr-Dp'") == 1,
             "invalid dr-Dp triggers SpecificCultures validation") &&
        ok;
    ok = expect(
             countCommandsContaining(invalidFixCommandsSecond, "Set-WinUserLanguageList") == 0,
             "invalid dr-Dp does not run add/remove fix commands") &&
        ok;
    ok = expect(
             countCommandsContaining(invalidFixCommandsSecond, "reg export") == 0,
             "invalid dr-Dp does not create additional backup exports") &&
        ok;
    ok = expect(invalidDryRunCode == static_cast<int>(ghost::core::ExitCode::FixError), "dry-run fails fast on invalid layout code") && ok;
    ok = expect(invalidDryRunCommandsAfter == invalidDryRunCommandsBefore, "invalid dry-run does not build plan through registry commands") && ok;
    ok = expect(
             countCommandsContaining(invalidDryRunCommands, "GetCultures([System.Globalization.CultureTypes]::SpecificCultures)") == 0,
             "invalid! is rejected by regex and does not trigger SpecificCultures validation") &&
        ok;
    return ok;
}

bool testApplicationReadFailureHandling()
{
    bool ok = true;

    {
        FakeCommandRunner runner;
        runner.rules.push_back({"GetCultures", 0, "0407=de-DE\n"});
        runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000407\n"});
        runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 1, "language query failed"});

        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions scanOptions;
        scanOptions.command = ghost::cli::CommandType::Scan;
        const int code = app.run(scanOptions);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::GeneralError),
                    "scan fails when installed language query fails") &&
            ok;
    }

    {
        FakeCommandRunner runner;
        runner.rules.push_back({"GetCultures", 1, "lcid map failed"});
        runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 0, "de-DE\n"});

        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions scanOptions;
        scanOptions.command = ghost::cli::CommandType::Scan;
        const int code = app.run(scanOptions);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::GeneralError),
                    "scan fails when registry LCID mapping fails") &&
            ok;
    }

    {
        FakeCommandRunner runner;
        runner.rules.push_back({"$layout = 'de-DE'", 0, "ok"});
        runner.rules.push_back({"GetCultures", 0, "0407=de-DE\n"});
        runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000407\n"});
        runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 1, "language query failed"});
        runner.rules.push_back({"reg export", 0, "ok"});
        runner.rules.push_back({"Set-WinUserLanguageList", 0, "ok"});

        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions dryRunOptions;
        dryRunOptions.command = ghost::cli::CommandType::Fix;
        dryRunOptions.layoutCode = "de-DE";
        dryRunOptions.dryRun = true;
        const std::size_t commandsBefore = runner.commands.size();
        const int code = app.run(dryRunOptions);
        const std::size_t commandsAfter = runner.commands.size();
        const std::vector<std::string> commands = commandSlice(runner.commands, commandsBefore, commandsAfter);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::FixError),
                    "fix --dry-run aborts when installed language query fails") &&
            ok;
        ok = expect(
                 countCommandsContaining(commands, "reg export") == 0,
                 "fix --dry-run does not create backup when installed language read fails") &&
            ok;
    }

    {
        FakeCommandRunner runner;
        runner.rules.push_back({"$layout = 'de-DE'", 0, "ok"});
        runner.rules.push_back({"GetCultures", 0, "0407=de-DE\n"});
        runner.rules.push_back({"reg query", 0, "    1    REG_SZ    00000407\n"});
        runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 1, "language query failed"});
        runner.rules.push_back({"reg export", 0, "ok"});

        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions fixOptions;
        fixOptions.command = ghost::cli::CommandType::Fix;
        fixOptions.layoutCode = "de-DE";
        const std::size_t commandsBefore = runner.commands.size();
        const int code = app.run(fixOptions);
        const std::size_t commandsAfter = runner.commands.size();
        const std::vector<std::string> commands = commandSlice(runner.commands, commandsBefore, commandsAfter);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::FixError),
                    "fix aborts before backup when installed language query fails") &&
            ok;
        ok = expect(
                 countCommandsContaining(commands, "reg export") == 0,
                 "fix does not create backup when installed language read fails") &&
            ok;
    }

    {
        FakeCommandRunner runner;
        runner.rules.push_back({"$layout = 'de-DE'", 0, "ok"});
        runner.rules.push_back({"GetCultures", 1, "lcid map failed"});
        runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 0, "de-DE\n"});
        runner.rules.push_back({"reg export", 0, "ok"});

        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions fixOptions;
        fixOptions.command = ghost::cli::CommandType::Fix;
        fixOptions.layoutCode = "de-DE";
        const std::size_t commandsBefore = runner.commands.size();
        const int code = app.run(fixOptions);
        const std::size_t commandsAfter = runner.commands.size();
        const std::vector<std::string> commands = commandSlice(runner.commands, commandsBefore, commandsAfter);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::FixError),
                    "fix aborts before backup when registry read fails") &&
            ok;
        ok = expect(
                 countCommandsContaining(commands, "reg export") == 0,
                 "fix does not create backup when registry read fails") &&
            ok;
    }

    {
        struct FinalVerificationFailRunner final : ghost::platform::ICommandRunner
        {
            mutable std::vector<std::string> commands;
            mutable int mappingReadCount{0};

            ghost::platform::CommandResult run(const std::string& command) const override
            {
                commands.push_back(command);

                if (command.find("$layout = 'de-DE'") != std::string::npos)
                {
                    return {0, "ok"};
                }

                if (command.find("(Get-WinUserLanguageList).LanguageTag") != std::string::npos)
                {
                    return {0, "en-US\n"};
                }

                if (command.find("'{0:X4}={1}' -f $_.LCID, $_.Name") != std::string::npos)
                {
                    ++mappingReadCount;
                    if (mappingReadCount >= 3)
                    {
                        return {1, "final verification mapping failed"};
                    }
                    return {0, "0409=en-US\n0407=de-DE\n"};
                }

                if (command.find("reg query") != std::string::npos)
                {
                    return {0, "    1    REG_SZ    00000407\n"};
                }

                if (command.find("reg export") != std::string::npos)
                {
                    const std::string exportPath = FakeCommandRunner::extractQuoted(command, 1);
                    if (!exportPath.empty())
                    {
                        std::ofstream out(exportPath, std::ios::binary | std::ios::trunc);
                        const std::u16string payload =
                            u"\uFEFFWindows Registry Editor Version 5.00\r\n\r\n"
                            u"[HKEY_CURRENT_USER\\Keyboard Layout\\Preload]\r\n"
                            u"\"1\"=\"00000407\"\r\n";
                        out.write(
                            reinterpret_cast<const char*>(payload.data()),
                            static_cast<std::streamsize>(payload.size() * sizeof(char16_t)));
                    }
                    return {0, "ok"};
                }

                if (command.find("reg delete") != std::string::npos)
                {
                    return {0, "ok"};
                }

                if (command.find("Set-WinUserLanguageList") != std::string::npos)
                {
                    return {0, "ok"};
                }

                return {};
            }
        };

        FinalVerificationFailRunner runner;
        ghost::core::BackupService backupService(&runner);
        ghost::platform::RegistryService registryService(&runner);
        ghost::platform::InstalledLanguageService installedLanguageService(&runner);
        ghost::core::LayoutFixService layoutFixService(&runner);
        ghost::report::ReportPrinter printer;
        FakePrivilegeService admin;
        ghost::app::ApplicationService app(
            admin,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);

        ghost::cli::CliOptions fixOptions;
        fixOptions.command = ghost::cli::CommandType::Fix;
        fixOptions.layoutCode = "de-DE";
        const int code = app.run(fixOptions);
        ok = expect(code == static_cast<int>(ghost::core::ExitCode::FixError),
                    "fix does not report success when final registry verification fails") &&
            ok;
    }

    return ok;
}

int main()
{
    bool ok = true;
    ok = testBackupPathFormat() && ok;
    ok = testBackupAggregatesAllBranches() && ok;
    ok = testBackupPartialFailure() && ok;
    ok = testBackupCreatesDirectoryWhenMissing() && ok;
    ok = testRestoreBackupValidationAndFailure() && ok;
    ok = testLayoutFixFailurePathsAndScanCases() && ok;
    ok = testRegistryFailures() && ok;
    ok = testRegistryMissingBranchIsSkipped() && ok;
    ok = testInstalledLanguageParsing() && ok;
    ok = testInstalledLanguageEmptyOnFailure() && ok;
    ok = testFixRejectsNonGhostLayoutAndAllowsGhostLayout() && ok;
    ok = testCliAndNoAdmin() && ok;
    ok = testApplicationReadFailureHandling() && ok;

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] Command workflow tests completed\n";
    return 0;
}
