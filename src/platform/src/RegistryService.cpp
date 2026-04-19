#include <src/platform/RegistryService.h>

#include <src/platform/SystemCommandRunner.h>
#include <src/platform/RegistryBranches.h>

#include <sstream>
#include <cctype>
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

struct LcidMapResult
{
    bool success{false};
    std::unordered_map<std::string, std::string> mapping;
    std::string error;
};

struct RegistrySnapshotsResult
{
    bool success{false};
    std::vector<RegistrySnapshot> values;
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
    const ghost::platform::CommandResult result = runner.run(
        "powershell -NoProfile -Command "
        "\"[System.Globalization.CultureInfo]::GetCultures("
        "[System.Globalization.CultureTypes]::SpecificCultures"
        ") | ForEach-Object { '{0:X4}={1}' -f $_.LCID, $_.Name }\"");

    LcidMapResult mappingResult;
    if (result.exitCode != 0)
    {
        mappingResult.success = false;
        mappingResult.error = result.outputText.empty() ? "failed to build LCID mapping" : result.outputText;
        return mappingResult;
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
            mappingResult.mapping[lcidHex] = languageTag;
        }
    }

    if (mappingResult.mapping.empty())
    {
        mappingResult.success = false;
        mappingResult.error = "failed to build LCID mapping: command returned no valid values";
        return mappingResult;
    }

    mappingResult.success = true;
    return mappingResult;
}

std::string extractLanguageIdHex(const std::string& hkl)
{
    if (hkl.size() < 4)
    {
        return {};
    }

    std::string lcidHex = hkl.substr(hkl.size() - 4);
    for (char& symbol : lcidHex)
    {
        symbol = static_cast<char>(std::toupper(static_cast<unsigned char>(symbol)));
    }

    return lcidHex;
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

RegistrySnapshotsResult readRegistrySnapshots(const ghost::platform::ICommandRunner& runner)
{
    const LcidMapResult lcidMapResult = buildLcidToLanguageTagMap(runner);
    RegistrySnapshotsResult snapshotsResult;
    if (!lcidMapResult.success)
    {
        snapshotsResult.success = false;
        snapshotsResult.error = lcidMapResult.error;
        return snapshotsResult;
    }

    for (const std::string& branchPath : ghost::platform::kRegistryBranches)
    {
        const ghost::platform::CommandResult query = runner.run("reg query \"" + branchPath + "\"");
        if (query.exitCode != 0)
        {
            const std::string& output = query.outputText;
            const bool isMissingBranch =
                output.find("unable to find the specified registry key or value") != std::string::npos ||
                output.find("The system was unable to find the specified registry key or value") != std::string::npos ||
                output.find("cannot find the file specified") != std::string::npos ||
                output.find("не удается найти указанный раздел реестра или параметр") != std::string::npos ||
                output.find("Системе не удается найти указанный путь") != std::string::npos;
            if (isMissingBranch)
            {
                continue;
            }

            snapshotsResult.success = false;
            snapshotsResult.error =
                "failed to query registry branch " + branchPath +
                (query.outputText.empty() ? std::string() : ": " + query.outputText);
            return snapshotsResult;
        }

        const std::vector<RegistrySnapshot> parsed =
            parseRegistryEntries(query.outputText, branchPath, lcidMapResult.mapping);
        snapshotsResult.values.insert(snapshotsResult.values.end(), parsed.begin(), parsed.end());
    }

    snapshotsResult.success = true;
    return snapshotsResult;
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

RegistryMatchesResult RegistryService::findLayoutMatches(const std::string& layoutCode) const
{
    const RegistrySnapshotsResult snapshotsResult = readRegistrySnapshots(*runner_);
    RegistryMatchesResult matchesResult;
    if (!snapshotsResult.success)
    {
        matchesResult.success = false;
        matchesResult.error = snapshotsResult.error;
        return matchesResult;
    }

    for (const RegistrySnapshot& snapshot : snapshotsResult.values)
    {
        if (snapshot.layoutCode != layoutCode)
        {
            continue;
        }

        RegistryMatch match;
        match.branchPath = snapshot.branchPath;
        match.valueName = snapshot.valueName;
        match.valueData = snapshot.valueData;
        matchesResult.values.push_back(match);
    }

    matchesResult.success = true;
    return matchesResult;
}

RegistryLayoutsResult RegistryService::listLayoutCodesFromRegistry() const
{
    const RegistrySnapshotsResult snapshotsResult = readRegistrySnapshots(*runner_);
    RegistryLayoutsResult layoutsResult;
    if (!snapshotsResult.success)
    {
        layoutsResult.success = false;
        layoutsResult.error = snapshotsResult.error;
        return layoutsResult;
    }

    std::unordered_set<std::string> seenLayouts;
    for (const RegistrySnapshot& snapshot : snapshotsResult.values)
    {
        if (seenLayouts.insert(snapshot.layoutCode).second)
        {
            layoutsResult.values.push_back(snapshot.layoutCode);
        }
    }

    layoutsResult.success = true;
    return layoutsResult;
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
