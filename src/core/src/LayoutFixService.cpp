#include <src/core/LayoutFixService.h>

#include <unordered_set>

namespace ghost::core
{

ScanResult LayoutFixService::scan(
    const std::vector<std::string>& registryLayouts,
    const std::vector<std::string>& installedLayouts) const
{
    ScanResult result;
    result.registryLayouts = registryLayouts;
    result.installedLayouts = installedLayouts;

    const std::unordered_set<std::string> installedSet(installedLayouts.begin(), installedLayouts.end());
    for (const std::string& layoutCode : registryLayouts)
    {
        if (installedSet.find(layoutCode) == installedSet.end())
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
