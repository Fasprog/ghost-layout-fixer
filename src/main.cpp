#include <src/cli/CliParser.h>
#include <src/core/BackupService.h>
#include <src/core/ExitCodes.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/PrivilegeService.h>
#include <src/platform/RegistryService.h>
#include <src/report/ReportPrinter.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{

std::string joinLayouts(const std::vector<std::string>& layouts)
{
    if (layouts.empty())
    {
        return "<empty>";
    }

    std::string result;
    for (std::size_t index = 0; index < layouts.size(); ++index)
    {
        result += layouts[index];
        if (index + 1 < layouts.size())
        {
            result += ", ";
        }
    }

    return result;
}

std::vector<std::string> toMatchSummaries(const std::vector<ghost::platform::RegistryMatch>& matches)
{
    std::vector<std::string> summaries;
    for (const ghost::platform::RegistryMatch& match : matches)
    {
        summaries.push_back(match.location + " -> " + match.valueName + " = " + match.valueData);
    }

    return summaries;
}

void printHelp(const ghost::report::ReportPrinter& printer)
{
    printer.print("ghost-layout-fixer usage:");
    printer.print("  ghost-layout-fixer --help");
    printer.print("  ghost-layout-fixer -h");
    printer.print("  ghost-layout-fixer scan");
    printer.print("  ghost-layout-fixer backup");
    printer.print("  ghost-layout-fixer fix --layout <language-tag> --dry-run");
    printer.print("  ghost-layout-fixer fix --layout <language-tag>");
    printer.print("  ghost-layout-fixer restore --file C:\\path\\backup.reg");
}

int handleScan(
    const ghost::platform::RegistryService& registryService,
    const ghost::platform::InstalledLanguageService& installedLanguageService,
    const ghost::core::LayoutFixService& layoutFixService,
    const ghost::report::ReportPrinter& printer)
{
    const std::vector<std::string> registryLayouts = registryService.listLayoutCodesFromRegistry();
    const std::vector<std::string> installedLayouts = installedLanguageService.listInstalledLayoutCodes();
    const ghost::core::ScanResult scanResult = layoutFixService.scan(registryLayouts, installedLayouts);

    printer.print("[scan] registry layouts: " + joinLayouts(scanResult.registryLayouts));
    printer.print("[scan] installed layouts: " + joinLayouts(scanResult.installedLayouts));
    printer.print("[scan] ghost layouts: " + joinLayouts(scanResult.ghostLayouts));
    printer.print("[scan] status: " + scanResult.status);

    return static_cast<int>(ghost::core::ExitCode::Success);
}

int handleBackup(
    const ghost::core::BackupService& backupService,
    const ghost::report::ReportPrinter& printer)
{
    const std::string backupPath = backupService.makeBackupPath();
    const ghost::core::BackupReport report = backupService.createBackup(backupPath);

    for (const std::string& command : report.executedCommands)
    {
        printer.print("[backup] command: " + command);
    }

    if (!report.success)
    {
        for (const std::string& error : report.errors)
        {
            printer.print("[backup] error: " + error);
        }
        return static_cast<int>(ghost::core::ExitCode::BackupError);
    }

    printer.print("[backup] created: " + report.backupPath);
    return static_cast<int>(ghost::core::ExitCode::Success);
}

int handleFixDryRun(
    const ghost::cli::CliOptions& options,
    const ghost::core::BackupService& backupService,
    const ghost::platform::RegistryService& registryService,
    const ghost::core::LayoutFixService& layoutFixService,
    const ghost::report::ReportPrinter& printer)
{
    if (!options.layoutCode.has_value())
    {
        printer.print("fix --dry-run requires --layout");
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    const std::string backupPath = backupService.makeBackupPath();
    const std::vector<ghost::platform::RegistryMatch> matches =
        registryService.findLayoutMatches(*options.layoutCode);
    const ghost::core::FixPlan plan =
        layoutFixService.buildDryRunPlan(*options.layoutCode, toMatchSummaries(matches), backupPath);

    printer.print("[fix --dry-run] layout: " + *options.layoutCode);
    for (const std::string& step : plan.plannedSteps)
    {
        printer.print("[fix --dry-run] step: " + step);
    }

    return static_cast<int>(ghost::core::ExitCode::Success);
}

int handleFix(
    const ghost::cli::CliOptions& options,
    const ghost::core::BackupService& backupService,
    const ghost::platform::RegistryService& registryService,
    const ghost::platform::InstalledLanguageService& installedLanguageService,
    const ghost::core::LayoutFixService& layoutFixService,
    const ghost::report::ReportPrinter& printer)
{
    if (!options.layoutCode.has_value())
    {
        printer.print("fix requires --layout");
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    const std::string backupPath = backupService.makeBackupPath();
    const ghost::core::BackupReport backupReport = backupService.createBackup(backupPath);
    for (const std::string& command : backupReport.executedCommands)
    {
        printer.print("[fix] backup command: " + command);
    }

    if (!backupReport.success)
    {
        for (const std::string& error : backupReport.errors)
        {
            printer.print("[fix] backup error: " + error);
        }
        return static_cast<int>(ghost::core::ExitCode::BackupError);
    }

    const std::vector<ghost::platform::RegistryMatch> matches =
        registryService.findLayoutMatches(*options.layoutCode);
    const ghost::core::FixReport fixReport =
        layoutFixService.executeFix(*options.layoutCode, matches, backupPath, registryService);

    for (const std::string& step : fixReport.executedSteps)
    {
        printer.print("[fix] step: " + step);
    }
    for (const std::string& error : fixReport.errors)
    {
        printer.print("[fix] error: " + error);
    }

    const std::vector<std::string> registryLayouts = registryService.listLayoutCodesFromRegistry();
    const std::vector<std::string> installedLayouts = installedLanguageService.listInstalledLayoutCodes();
    const ghost::core::ScanResult scanResult = layoutFixService.scan(registryLayouts, installedLayouts);
    printer.print("[fix] post-scan ghost layouts: " + joinLayouts(scanResult.ghostLayouts));
    printer.print("[fix] recommendation: reboot or sign out to apply language switcher state");

    if (!fixReport.success)
    {
        return static_cast<int>(ghost::core::ExitCode::FixError);
    }

    return static_cast<int>(ghost::core::ExitCode::Success);
}

int handleRestore(
    const ghost::cli::CliOptions& options,
    const ghost::core::BackupService& backupService,
    const ghost::report::ReportPrinter& printer)
{
    if (!options.restoreFile.has_value())
    {
        printer.print("restore requires --file");
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    const ghost::core::RestoreReport restoreReport = backupService.restoreBackup(*options.restoreFile);
    for (const std::string& command : restoreReport.executedCommands)
    {
        printer.print("[restore] command: " + command);
    }
    for (const std::string& error : restoreReport.errors)
    {
        printer.print("[restore] error: " + error);
    }

    if (!restoreReport.success)
    {
        return static_cast<int>(ghost::core::ExitCode::RestoreError);
    }

    printer.print("[restore] restored from: " + restoreReport.sourcePath);
    return static_cast<int>(ghost::core::ExitCode::Success);
}

} // namespace

int main(int argc, const char* const argv[])
{
    const ghost::cli::CliParser parser;
    const ghost::cli::CliOptions options = parser.parse(argc, argv);

    const ghost::report::ReportPrinter printer;
    if (options.showHelp)
    {
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (!options.parseErrors.empty())
    {
        for (const std::string& error : options.parseErrors)
        {
            printer.print("[error] " + error);
        }
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    if (options.command == ghost::cli::CommandType::Unknown)
    {
        printer.print("[error] command is not implemented");
        printHelp(printer);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    if (options.command == ghost::cli::CommandType::Scan)
    {
        const ghost::platform::RegistryService registryService;
        const ghost::platform::InstalledLanguageService installedLanguageService;
        const ghost::core::LayoutFixService layoutFixService;
        return handleScan(registryService, installedLanguageService, layoutFixService, printer);
    }

    const ghost::platform::PrivilegeService privilegeService;
    const ghost::core::BackupService backupService;
    const ghost::platform::RegistryService registryService;
    const ghost::platform::InstalledLanguageService installedLanguageService;
    const ghost::core::LayoutFixService layoutFixService;

    if (!privilegeService.isRunningAsAdmin())
    {
        printer.print("administrator privileges are required");
        return static_cast<int>(ghost::core::ExitCode::InsufficientPrivileges);
    }

    if (options.command == ghost::cli::CommandType::Backup)
    {
        return handleBackup(backupService, printer);
    }

    if (options.command == ghost::cli::CommandType::Fix && options.dryRun)
    {
        return handleFixDryRun(options, backupService, registryService, layoutFixService, printer);
    }

    if (options.command == ghost::cli::CommandType::Fix)
    {
        return handleFix(
            options,
            backupService,
            registryService,
            installedLanguageService,
            layoutFixService,
            printer);
    }

    if (options.command == ghost::cli::CommandType::Restore)
    {
        return handleRestore(options, backupService, printer);
    }

    printer.print("[error] command is not supported");
    return static_cast<int>(ghost::core::ExitCode::GeneralError);
}
