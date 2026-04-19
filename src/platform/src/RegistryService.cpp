#include <src/platform/RegistryService.h>

#include <src/platform/SystemCommandRunner.h>
#include <src/platform/RegistryBranches.h>

#include <sstream>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{

struct RegistrySnapshot
{
    std::string branchPath;
    std::string valueName;
    std::string valueData;
    std::string layoutCode;
};

struct LcidMapResult
{
    bool success{false};
    std::unordered_map<std::string, std::string> mapping;
    std::string error;
};

struct RegistryReadResult
{
    bool success{false};
    std::vector<RegistrySnapshot> snapshots;
    std::string error;
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

LcidMapResult buildLcidToLanguageTagMap(const ghost::platform::ICommandRunner& runner)
{
    LcidMapResult mapResult;
    const ghost::platform::CommandResult result = runner.run(
        "powershell -NoProfile -Command "
        "\"[System.Globalization.CultureInfo]::GetCultures("
        "[System.Globalization.CultureTypes]::SpecificCultures"
        ") | ForEach-Object { '{0:X4}={1}' -f $_.LCID, $_.Name }\"");

    if (result.exitCode != 0)
    {
        mapResult.error = "failed to build LCID mapping (exit code " + std::to_string(result.exitCode) + "): " + result.outputText;
        return mapResult;
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
            mapResult.mapping[lcidHex] = languageTag;
        }
    }

    if (mapResult.mapping.empty())
    {
        mapResult.error = "failed to build LCID mapping: empty result";
        return mapResult;
    }

    mapResult.success = true;
    return mapResult;
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

RegistryReadResult readRegistrySnapshots(const ghost::platform::ICommandRunner& runner)
{
    RegistryReadResult readResult;
    const LcidMapResult mapResult = buildLcidToLanguageTagMap(runner);
    if (!mapResult.success)
    {
        readResult.error = mapResult.error;
        return readResult;
    }

    for (const std::string& branchPath : ghost::platform::kRegistryBranches)
    {
        const ghost::platform::CommandResult query = runner.run("reg query \"" + branchPath + "\"");
        if (query.exitCode != 0)
        {
            readResult.error = "failed to query registry branch " + branchPath + " (exit code " +
                               std::to_string(query.exitCode) + "): " + query.outputText;
            return readResult;
        }

        const std::vector<RegistrySnapshot> parsed = parseRegistryEntries(query.outputText, branchPath, mapResult.mapping);
        readResult.snapshots.insert(readResult.snapshots.end(), parsed.begin(), parsed.end());
    }

    readResult.success = true;
    return readResult;
}

bool isSafeRegistryBranchPath(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    for (const char symbol : value)
    {
        const unsigned char normalized = static_cast<unsigned char>(symbol);
        if (symbol == '"' || symbol == '\r' || symbol == '\n')
        {
            return false;
        }

        if (!(std::isalnum(normalized) || symbol == '\\' || symbol == '_' || symbol == '-' || symbol == ' ' ||
              symbol == '.'))
        {
            return false;
        }
    }

    return true;
}

bool isSafeRegistryValueName(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    for (const char symbol : value)
    {
        const unsigned char normalized = static_cast<unsigned char>(symbol);
        if (symbol == '"' || symbol == '\r' || symbol == '\n')
        {
            return false;
        }

        if (!(std::isalnum(normalized) || symbol == '_' || symbol == '-' || symbol == ' '))
        {
            return false;
        }
    }

    return true;
}

} // namespace

namespace ghost::platform
{

RegistryService::RegistryService(const ICommandRunner* runner) : runner_(runner != nullptr ? runner : &defaultRunner()) {}

std::vector<RegistryMatch> RegistryService::findLayoutMatches(const std::string& layoutCode) const
{
    const RegistryMatchesResult safeResult = findLayoutMatchesSafe(layoutCode);
    return safeResult.matches;
}

RegistryMatchesResult RegistryService::findLayoutMatchesSafe(const std::string& layoutCode) const
{
    RegistryMatchesResult safeResult;
    const RegistryReadResult readResult = readRegistrySnapshots(*runner_);
    if (!readResult.success)
    {
        safeResult.error = readResult.error;
        return safeResult;
    }

    std::vector<RegistryMatch> matches;
    for (const RegistrySnapshot& snapshot : readResult.snapshots)
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

    safeResult.matches = std::move(matches);
    safeResult.success = true;
    return safeResult;
}

std::vector<std::string> RegistryService::listLayoutCodesFromRegistry() const
{
    const RegistryLayoutsResult safeResult = listLayoutCodesFromRegistrySafe();
    return safeResult.layouts;
}

RegistryLayoutsResult RegistryService::listLayoutCodesFromRegistrySafe() const
{
    RegistryLayoutsResult safeResult;
    const RegistryReadResult readResult = readRegistrySnapshots(*runner_);
    if (!readResult.success)
    {
        safeResult.error = readResult.error;
        return safeResult;
    }

    std::unordered_set<std::string> seenLayouts;
    std::vector<std::string> layouts;
    for (const RegistrySnapshot& snapshot : readResult.snapshots)
    {
        if (seenLayouts.insert(snapshot.layoutCode).second)
        {
            layouts.push_back(snapshot.layoutCode);
        }
    }

    safeResult.layouts = std::move(layouts);
    safeResult.success = true;
    return safeResult;
}

std::vector<std::string> RegistryService::deleteMatches(const std::vector<RegistryMatch>& matches) const
{
    std::vector<std::string> errors;

    for (const RegistryMatch& match : matches)
    {
        if (!isSafeRegistryBranchPath(match.branchPath) || !isSafeRegistryValueName(match.valueName))
        {
            errors.push_back(
                "failed to delete " + match.branchPath + "\\" + match.valueName + ": invalid registry path/value");
            continue;
        }

        const std::string command = "reg delete \"" + match.branchPath + "\" /v \"" + match.valueName + "\" /f";
        const ghost::platform::CommandResult result = runner_->run(command);
        if (result.exitCode != 0)
        {
            errors.push_back("failed to delete " + match.branchPath + "\\" + match.valueName + ": " + result.outputText);
        }
    }

    return errors;
}

} // namespace ghost::platform
