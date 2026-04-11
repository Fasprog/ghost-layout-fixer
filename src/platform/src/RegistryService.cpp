#include <src/platform/RegistryService.h>

#include <src/platform/SystemCommandRunner.h>
#include <src/platform/RegistryBranches.h>

#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{

struct RegistrySnapshot
{
    std::string branchPath;
    std::string valueName;
    std::string valueData;
    std::string layoutCode;
};

const ghost::platform::ICommandRunner& defaultRunner()
{
    static const ghost::platform::SystemCommandRunner runner;
    return runner;
}

std::string trimWhitespace(const std::string& value)
{
    std::string trimmed;
    for (const char symbol : value)
    {
        if (symbol != ' ' && symbol != '\t' && symbol != '\r')
        {
            trimmed.push_back(symbol);
        }
    }

    return trimmed;
}

std::unordered_map<std::string, std::string> buildLcidToLanguageTagMap(const ghost::platform::ICommandRunner& runner)
{
    const ghost::platform::CommandResult result = runner.run(
        "powershell -NoProfile -Command "
        "\"[System.Globalization.CultureInfo]::GetCultures("
        "[System.Globalization.CultureTypes]::SpecificCultures"
        ") | ForEach-Object { '{0:X4}={1}' -f $_.LCID, $_.Name }\"");

    std::unordered_map<std::string, std::string> mapping;
    if (result.exitCode != 0)
    {
        return mapping;
    }

    std::string line;
    std::stringstream stream(result.outputText);
    while (std::getline(stream, line))
    {
        const std::size_t separatorPos = line.find('=');
        if (separatorPos == std::string::npos || separatorPos == 0 || separatorPos + 1 >= line.size())
        {
            continue;
        }

        const std::string lcidHex = trimWhitespace(line.substr(0, separatorPos));
        const std::string languageTag = trimWhitespace(line.substr(separatorPos + 1));
        if (!lcidHex.empty() && !languageTag.empty())
        {
            mapping[lcidHex] = languageTag;
        }
    }

    return mapping;
}

std::string extractLanguageIdHex(const std::string& hkl)
{
    if (hkl.size() < 4)
    {
        return {};
    }

    return hkl.substr(hkl.size() - 4);
}

std::string hklToLayoutCode(
    const std::string& hkl,
    const std::unordered_map<std::string, std::string>& lcidMapping)
{
    const std::string lcidHex = extractLanguageIdHex(hkl);
    const auto found = lcidMapping.find(lcidHex);
    if (found == lcidMapping.end())
    {
        return {};
    }

    return found->second;
}

std::vector<RegistrySnapshot> parseRegistryEntries(
    const std::string& regOutput,
    const std::string& branchPath,
    const std::unordered_map<std::string, std::string>& lcidMapping)
{
    std::vector<RegistrySnapshot> entries;
    std::stringstream stream(regOutput);
    std::string line;
    while (std::getline(stream, line))
    {
        const std::size_t tokenPos = line.find("REG_SZ");
        if (tokenPos == std::string::npos)
        {
            continue;
        }

        const std::string valueName = trimWhitespace(line.substr(0, tokenPos));
        const std::string valueData = trimWhitespace(line.substr(tokenPos + std::string_view("REG_SZ").size()));
        const std::string layoutCode = hklToLayoutCode(valueData, lcidMapping);
        if (layoutCode.empty())
        {
            continue;
        }

        RegistrySnapshot snapshot;
        snapshot.branchPath = branchPath;
        snapshot.valueName = valueName;
        snapshot.valueData = valueData;
        snapshot.layoutCode = layoutCode;
        entries.push_back(snapshot);
    }

    return entries;
}

std::vector<RegistrySnapshot> readRegistrySnapshots(const ghost::platform::ICommandRunner& runner)
{
    const std::unordered_map<std::string, std::string> lcidMapping = buildLcidToLanguageTagMap(runner);
    std::vector<RegistrySnapshot> snapshots;
    if (lcidMapping.empty())
    {
        return snapshots;
    }

    for (const std::string& branchPath : ghost::platform::kRegistryBranches)
    {
        const ghost::platform::CommandResult query = runner.run("reg query \"" + branchPath + "\"");
        if (query.exitCode != 0)
        {
            continue;
        }

        const std::vector<RegistrySnapshot> parsed = parseRegistryEntries(query.outputText, branchPath, lcidMapping);
        snapshots.insert(snapshots.end(), parsed.begin(), parsed.end());
    }

    return snapshots;
}

} // namespace

namespace ghost::platform
{

RegistryService::RegistryService(const ICommandRunner* runner) : runner_(runner != nullptr ? runner : &defaultRunner()) {}

std::vector<RegistryMatch> RegistryService::findLayoutMatches(const std::string& layoutCode) const
{
    const std::vector<RegistrySnapshot> snapshots = readRegistrySnapshots(*runner_);
    std::vector<RegistryMatch> matches;
    for (const RegistrySnapshot& snapshot : snapshots)
    {
        if (snapshot.layoutCode != layoutCode)
        {
            continue;
        }

        RegistryMatch match;
        match.branchPath = snapshot.branchPath;
        match.valueName = snapshot.valueName;
        match.valueData = snapshot.valueData;
        matches.push_back(match);
    }

    return matches;
}

std::vector<std::string> RegistryService::listLayoutCodesFromRegistry() const
{
    const std::vector<RegistrySnapshot> snapshots = readRegistrySnapshots(*runner_);
    std::unordered_set<std::string> uniqueLayouts;
    for (const RegistrySnapshot& snapshot : snapshots)
    {
        uniqueLayouts.insert(snapshot.layoutCode);
    }

    return std::vector<std::string>(uniqueLayouts.begin(), uniqueLayouts.end());
}

std::vector<std::string> RegistryService::deleteMatches(const std::vector<RegistryMatch>& matches) const
{
    std::vector<std::string> errors;

    for (const RegistryMatch& match : matches)
    {
        const std::string command =
            "reg delete \"" + match.branchPath + "\" /v \"" + match.valueName + "\" /f";
        const ghost::platform::CommandResult result = runner_->run(command);
        if (result.exitCode != 0)
        {
            errors.push_back("failed to delete " + match.branchPath + "\\" + match.valueName + ": " + result.outputText);
        }
    }

    return errors;
}

} // namespace ghost::platform
