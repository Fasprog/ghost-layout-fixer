#include <src/core/LayoutFixService.h>

#include <src/platform/SystemCommandRunner.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace
{
const ghost::platform::ICommandRunner& defaultRunner()
{
    static const ghost::platform::SystemCommandRunner runner;
    return runner;
}

std::string normalizeCode(const std::string& code)
{
    std::string normalized = code;
    std::replace(normalized.begin(), normalized.end(), '_', '-');
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char symbol)
        {
            return static_cast<char>(std::tolower(symbol));
        });
    return normalized;
}

bool isInstalled(
    const std::string& registryLayout,
    const std::unordered_set<std::string>& installedFullCodes)
{
    const std::string normalizedRegistry = normalizeCode(registryLayout);
    if (installedFullCodes.find(normalizedRegistry) != installedFullCodes.end())
    {
        return true;
    }

    const std::size_t registrySeparatorPos = normalizedRegistry.find('-');
    if (registrySeparatorPos == std::string::npos)
    {
        return false;
    }

    const std::string registryPrimary = normalizedRegistry.substr(0, registrySeparatorPos);
    return installedFullCodes.find(registryPrimary) != installedFullCodes.end();
}

bool matchesRequestedLayout(
    const std::string& requestedLayout,
    const std::string& registryLayout)
{
    return normalizeCode(requestedLayout) == normalizeCode(registryLayout);
}

bool isValidLanguageTag(const std::string& value)
{
    static const std::regex kLanguageTagPattern("^[A-Za-z]{2,3}(?:-[A-Za-z0-9]{2,8})*$");
    return std::regex_match(value, kLanguageTagPattern);
}

} // namespace

namespace ghost::core
{
LayoutFixService::LayoutFixService(const ghost::platform::ICommandRunner* runner)
    : runner_(runner != nullptr ? runner : &defaultRunner())
{
}

ScanResult LayoutFixService::scan(
    const std::vector<std::string>& registryLayouts,
    const std::vector<std::string>& installedLayouts) const
{
    ScanResult result;
    result.registryLayouts = registryLayouts;
    result.installedLayouts = installedLayouts;

    std::unordered_set<std::string> installedFullCodes;
    for (const std::string& installedLayout : installedLayouts)
    {
        installedFullCodes.insert(normalizeCode(installedLayout));
    }

    for (const std::string& layoutCode : registryLayouts)
    {
        if (!isInstalled(layoutCode, installedFullCodes))
        {
            result.ghostLayouts.push_back(layoutCode);
        }
    }

    if (result.ghostLayouts.empty())
    {
        result.status = "ghost layouts were not found";
    }
    else
    {
        result.status = "ghost layouts detected";
    }

    return result;
}

FixPlan LayoutFixService::buildDryRunPlan(
    const std::string& layoutCode,
    const std::vector<std::string>& registryMatchSummaries,
    const std::string& backupPath) const
{
    FixPlan plan;
    plan.plannedSteps.push_back("validate privileges and input arguments");
    plan.plannedSteps.push_back("create registry backup: " + backupPath);
    plan.plannedSteps.push_back("try standard add/remove cycle for layout: " + layoutCode);

    if (registryMatchSummaries.empty())
    {
        plan.plannedSteps.push_back("registry cleanup skipped: no matches for " + layoutCode);
    }
    else
    {
        for (const std::string& matchSummary : registryMatchSummaries)
        {
            plan.plannedSteps.push_back("remove registry value: " + matchSummary);
        }
    }

    plan.plannedSteps.push_back("run scan again to verify ghost layout state");
    return plan;
}

FixReport LayoutFixService::executeFix(
    const std::string& layoutCode,
    const std::string& backupPath) const
{
    FixReport report;
    report.backupPath = backupPath;
    report.executedSteps.push_back("backup created: " + backupPath);

    if (!isValidLayoutCodeFormat(layoutCode))
    {
        report.errors.push_back("invalid layout code: " + layoutCode);
        report.success = false;
        return report;
    }

    const std::string addCommand = "powershell -NoProfile -Command \"$list = Get-WinUserLanguageList; "
                                   "if (-not ($list.LanguageTag -contains '" + layoutCode + "')) { $list.Add('" + layoutCode +
                                    "'); Set-WinUserLanguageList $list -Force }\"";
    const ghost::platform::CommandResult addResult = runner_->run(addCommand);
    report.executedSteps.push_back("standard add command: " + addCommand);
    if (addResult.exitCode != 0)
    {
        report.errors.push_back("standard add failed: " + addResult.outputText);
    }

    const std::string removeCommand = "powershell -NoProfile -Command \"$list = Get-WinUserLanguageList; "
                                      "$filtered = @($list | Where-Object { $_.LanguageTag -ne '" +
                                      layoutCode + "' }); Set-WinUserLanguageList $filtered -Force\"";
    const ghost::platform::CommandResult removeResult = runner_->run(removeCommand);
    report.executedSteps.push_back("standard remove command: " + removeCommand);
    if (removeResult.exitCode != 0)
    {
        report.errors.push_back("standard remove failed: " + removeResult.outputText);
    }

    report.success = report.errors.empty();
    return report;
}

bool LayoutFixService::isValidLayoutCode(const std::string& layoutCode) const
{
    if (!isValidLayoutCodeFormat(layoutCode))
    {
        return false;
    }

    const std::string validateCommand =
        "powershell -NoProfile -Command \"$layout = '" + layoutCode +
        "'; $match = [System.Globalization.CultureInfo]::GetCultures([System.Globalization.CultureTypes]::SpecificCultures) | "
        "Where-Object { $_.Name -ieq $layout } | Select-Object -First 1; if ($null -eq $match) { exit 1 }\"";
    const ghost::platform::CommandResult validateResult = runner_->run(validateCommand);
    return validateResult.exitCode == 0;
}

bool LayoutFixService::isValidLayoutCodeFormat(const std::string& layoutCode) const
{
    return isValidLanguageTag(layoutCode);
}

bool LayoutFixService::isGhostLayout(
    const std::string& layoutCode,
    const std::vector<std::string>& registryLayouts,
    const std::vector<std::string>& installedLayouts) const
{
    std::unordered_set<std::string> installedFullCodes;
    for (const std::string& installedLayout : installedLayouts)
    {
        installedFullCodes.insert(normalizeCode(installedLayout));
    }

    for (const std::string& registryLayout : registryLayouts)
    {
        if (!matchesRequestedLayout(layoutCode, registryLayout))
        {
            continue;
        }

        if (!isInstalled(registryLayout, installedFullCodes))
        {
            return true;
        }
    }

    return false;
}

} // namespace ghost::core
