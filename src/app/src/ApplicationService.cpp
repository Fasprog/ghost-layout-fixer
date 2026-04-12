#include <src/app/ApplicationService.h>

#include <src/core/ExitCodes.h>

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
        summaries.push_back(match.branchPath + " -> " + match.valueName + " = " + match.valueData);
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
    printer.print("  ghost-layout-fixer restore --file .\\backups\\ghost-layout-backup-YYYYMMDD-HHMMSS.reg");
}

} // namespace

namespace ghost::app
{

ApplicationService::ApplicationService(
    const ghost::platform::IPrivilegeService& privilegeService,
    const ghost::core::BackupService& backupService,
    const ghost::platform::RegistryService& registryService,
    const ghost::platform::InstalledLanguageService& installedLanguageService,
    const ghost::core::LayoutFixService& layoutFixService,
    const ghost::report::ReportPrinter& printer)
    : privilegeService_(privilegeService),
      backupService_(backupService),
      registryService_(registryService),
      installedLanguageService_(installedLanguageService),
      layoutFixService_(layoutFixService),
      printer_(printer)
{
}

int ApplicationService::run(const ghost::cli::CliOptions& options) const
{
    if (options.showHelp)
    {
        printHelp(printer_);
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (!options.parseErrors.empty())
    {
        for (const std::string& error : options.parseErrors)
        {
            printer_.print("[error] " + error);
        }
        printHelp(printer_);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    if (options.command == ghost::cli::CommandType::Unknown)
    {
        printer_.print("[error] command is not implemented");
        printHelp(printer_);
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    if (options.command == ghost::cli::CommandType::Scan)
    {
        const std::vector<std::string> registryLayouts = registryService_.listLayoutCodesFromRegistry();
        const std::vector<std::string> installedLayouts = installedLanguageService_.listInstalledLayoutCodes();
        const ghost::core::ScanResult scanResult = layoutFixService_.scan(registryLayouts, installedLayouts);
        printer_.print("[scan] registry layouts: " + joinLayouts(scanResult.registryLayouts));
        printer_.print("[scan] installed language tags: " + joinLayouts(scanResult.installedLayouts));
        printer_.print("[scan] ghost layouts: " + joinLayouts(scanResult.ghostLayouts));
        printer_.print("[scan] status: " + scanResult.status);
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (!privilegeService_.isRunningAsAdmin())
    {
        printer_.print("administrator privileges are required for backup/fix/restore operations");
        return static_cast<int>(ghost::core::ExitCode::InsufficientPrivileges);
    }

    if (options.command == ghost::cli::CommandType::Backup)
    {
        const std::string backupPath = backupService_.makeBackupPath();
        const ghost::core::BackupReport report = backupService_.createBackup(backupPath);
        for (const std::string& command : report.executedCommands)
        {
            printer_.print("[backup] command: " + command);
        }
        if (!report.success)
        {
            for (const std::string& error : report.errors)
            {
                printer_.print("[backup] error: " + error);
            }
            return static_cast<int>(ghost::core::ExitCode::BackupError);
        }

        printer_.print("[backup] created: " + report.backupPath);
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (options.command == ghost::cli::CommandType::Fix && options.dryRun)
    {
        if (!options.layoutCode.has_value())
        {
            printer_.print("fix --dry-run requires --layout");
            printHelp(printer_);
            return static_cast<int>(ghost::core::ExitCode::GeneralError);
        }

        const std::string backupPath = backupService_.makeBackupPath();
        const std::vector<ghost::platform::RegistryMatch> matches =
            registryService_.findLayoutMatches(*options.layoutCode);
        const ghost::core::FixPlan plan =
            layoutFixService_.buildDryRunPlan(*options.layoutCode, toMatchSummaries(matches), backupPath);

        printer_.print("[fix --dry-run] layout: " + *options.layoutCode);
        for (const std::string& step : plan.plannedSteps)
        {
            printer_.print("[fix --dry-run] step: " + step);
        }
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (options.command == ghost::cli::CommandType::Fix)
    {
        if (!options.layoutCode.has_value())
        {
            printer_.print("fix requires --layout");
            printHelp(printer_);
            return static_cast<int>(ghost::core::ExitCode::GeneralError);
        }

        const std::string backupPath = backupService_.makeBackupPath();
        const ghost::core::BackupReport backupReport = backupService_.createBackup(backupPath);
        for (const std::string& command : backupReport.executedCommands)
        {
            printer_.print("[fix] backup command: " + command);
        }

        if (!backupReport.success)
        {
            for (const std::string& error : backupReport.errors)
            {
                printer_.print("[fix] backup error: " + error);
            }
            return static_cast<int>(ghost::core::ExitCode::BackupError);
        }

        ghost::core::FixReport fixReport =
            layoutFixService_.executeFix(*options.layoutCode, backupPath);

        for (const std::string& step : fixReport.executedSteps)
        {
            printer_.print("[fix] step: " + step);
        }
        for (const std::string& error : fixReport.errors)
        {
            printer_.print("[fix] error: " + error);
        }

        const std::vector<std::string> registryLayouts = registryService_.listLayoutCodesFromRegistry();
        const std::vector<std::string> installedLayouts = installedLanguageService_.listInstalledLayoutCodes();
        const ghost::core::ScanResult scanResult = layoutFixService_.scan(registryLayouts, installedLayouts);
        ghost::core::ScanResult finalScanResult = scanResult;
        const std::size_t stepsBeforeCleanup = fixReport.executedSteps.size();
        const std::size_t errorsBeforeCleanup = fixReport.errors.size();

        if (scanResult.ghostLayouts.empty())
        {
            fixReport.executedSteps.push_back("registry cleanup skipped: ghost layout not found after add/remove");
        }
        else
        {
            const std::vector<ghost::platform::RegistryMatch> actualMatches =
                registryService_.findLayoutMatches(*options.layoutCode);
            fixReport.executedSteps.push_back(
                "registry cleanup matches: " + std::to_string(actualMatches.size()));
            const std::vector<std::string> cleanupErrors = registryService_.deleteMatches(actualMatches);
            fixReport.errors.insert(fixReport.errors.end(), cleanupErrors.begin(), cleanupErrors.end());

            const std::vector<std::string> registryLayoutsAfterCleanup =
                registryService_.listLayoutCodesFromRegistry();
            const std::vector<std::string> installedLayoutsAfterCleanup =
                installedLanguageService_.listInstalledLayoutCodes();
            finalScanResult =
                layoutFixService_.scan(registryLayoutsAfterCleanup, installedLayoutsAfterCleanup);
            if (!finalScanResult.ghostLayouts.empty())
            {
                fixReport.executedSteps.push_back(
                    "registry cleanup completed but ghost layout is still present");
            }
        }

        fixReport.success = fixReport.errors.empty() && finalScanResult.ghostLayouts.empty();

        for (std::size_t index = stepsBeforeCleanup; index < fixReport.executedSteps.size(); ++index)
        {
            printer_.print("[fix] step: " + fixReport.executedSteps[index]);
        }
        for (std::size_t index = errorsBeforeCleanup; index < fixReport.errors.size(); ++index)
        {
            printer_.print("[fix] error: " + fixReport.errors[index]);
        }

        printer_.print("[fix] post-scan ghost layouts: " + joinLayouts(finalScanResult.ghostLayouts));
        printer_.print("[fix] recommendation: reboot or sign out to apply language switcher state");

        if (!fixReport.success)
        {
            return static_cast<int>(ghost::core::ExitCode::FixError);
        }

        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    if (options.command == ghost::cli::CommandType::Restore)
    {
        if (!options.restoreFile.has_value())
        {
            printer_.print("restore requires --file");
            printHelp(printer_);
            return static_cast<int>(ghost::core::ExitCode::GeneralError);
        }

        const ghost::core::RestoreReport restoreReport = backupService_.restoreBackup(*options.restoreFile);
        for (const std::string& command : restoreReport.executedCommands)
        {
            printer_.print("[restore] command: " + command);
        }
        for (const std::string& error : restoreReport.errors)
        {
            printer_.print("[restore] error: " + error);
        }

        if (!restoreReport.success)
        {
            return static_cast<int>(ghost::core::ExitCode::RestoreError);
        }

        printer_.print("[restore] restored from: " + restoreReport.sourcePath);
        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    printer_.print("[error] command is not supported");
    return static_cast<int>(ghost::core::ExitCode::GeneralError);
}

} // namespace ghost::app
