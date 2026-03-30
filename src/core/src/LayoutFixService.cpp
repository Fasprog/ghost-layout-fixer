#include <src/core/LayoutFixService.h>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace
{

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

std::string primaryLanguageTag(const std::string& code)
{
    const std::string normalized = normalizeCode(code);
    const std::size_t separatorPos = normalized.find('-');
    if (separatorPos == std::string::npos)
    {
        return normalized;
    }

    return normalized.substr(0, separatorPos);
}

bool isInstalled(
    const std::string& registryLayout,
    const std::unordered_set<std::string>& installedFullCodes,
    const std::unordered_set<std::string>& installedPrimaryCodes)
{
    const std::string normalizedRegistry = normalizeCode(registryLayout);
    if (installedFullCodes.find(normalizedRegistry) != installedFullCodes.end())
    {
        return true;
    }

    const std::string registryPrimary = primaryLanguageTag(registryLayout);
    return installedPrimaryCodes.find(registryPrimary) != installedPrimaryCodes.end();
}

} // namespace

namespace ghost::core
{

ScanResult LayoutFixService::scan(
    const std::vector<std::string>& registryLayouts,
    const std::vector<std::string>& installedLayouts) const
{
    ScanResult result;
    result.registryLayouts = registryLayouts;
    result.installedLayouts = installedLayouts;

    std::unordered_set<std::string> installedFullCodes;
    std::unordered_set<std::string> installedPrimaryCodes;
    for (const std::string& installedLayout : installedLayouts)
    {
        installedFullCodes.insert(normalizeCode(installedLayout));
        installedPrimaryCodes.insert(primaryLanguageTag(installedLayout));
    }

    for (const std::string& layoutCode : registryLayouts)
    {
        if (!isInstalled(layoutCode, installedFullCodes, installedPrimaryCodes))
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

} // namespace ghost::core
