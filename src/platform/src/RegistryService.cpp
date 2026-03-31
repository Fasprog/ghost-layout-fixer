#include <src/platform/RegistryService.h>

#include <src/platform/SystemCommandRunner.h>

#include <cstdlib>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{

struct RegistrySnapshot
{
    std::string location;
    std::string valueName;
    std::string valueData;
    std::string layoutCode;
};

std::vector<std::string> splitCsv(const std::string& csv)
{
    std::vector<std::string> result;
    std::stringstream stream(csv);
    std::string part;
    while (std::getline(stream, part, ','))
    {
        if (!part.empty())
        {
            result.push_back(part);
        }
    }

    return result;
}

std::string hklToLayoutCode(const std::string& hkl)
{
    static const std::unordered_map<std::string, std::string> mapping = {
        {"00000409", "en-US"},
        {"00000809", "en-GB"},
        {"00000419", "ru-RU"},
        {"00000422", "uk-UA"}
    };

    const auto found = mapping.find(hkl);
    if (found == mapping.end())
    {
        return {};
    }

    return found->second;
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

std::vector<std::pair<std::string, std::string>> registryBranches()
{
    return {
        {"HKCU\\Keyboard Layout\\Preload", "HKEY_CURRENT_USER\\Keyboard Layout\\Preload"},
        {"HKCU\\Keyboard Layout\\Substitutes", "HKEY_CURRENT_USER\\Keyboard Layout\\Substitutes"},
        {"HKU\\.DEFAULT\\Keyboard Layout\\Preload", "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Preload"},
        {"HKU\\.DEFAULT\\Keyboard Layout\\Substitutes", "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Substitutes"},
        {"HKCU\\Control Panel\\International\\User Profile", "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile"}
    };
}

std::vector<RegistrySnapshot> parseRegistryEntries(const std::string& regOutput, const std::string& location)
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
        const std::string layoutCode = hklToLayoutCode(valueData);
        if (layoutCode.empty())
        {
            continue;
        }

        RegistrySnapshot snapshot;
        snapshot.location = location;
        snapshot.valueName = valueName;
        snapshot.valueData = valueData;
        snapshot.layoutCode = layoutCode;
        entries.push_back(snapshot);
    }

    return entries;
}

std::vector<RegistrySnapshot> readRegistrySnapshots()
{
    const ghost::platform::SystemCommandRunner runner;
    std::vector<RegistrySnapshot> snapshots;

    for (const auto& [locationAlias, branchPath] : registryBranches())
    {
        const ghost::platform::CommandResult query = runner.run("reg query \"" + branchPath + "\"");
        const std::vector<RegistrySnapshot> parsed = parseRegistryEntries(query.stdoutText, locationAlias);
        snapshots.insert(snapshots.end(), parsed.begin(), parsed.end());
    }

    return snapshots;
}

} // namespace

namespace ghost::platform
{

std::vector<RegistryMatch> RegistryService::findLayoutMatches(const std::string& layoutCode) const
{
    const std::vector<RegistrySnapshot> snapshots = readRegistrySnapshots();
    std::vector<RegistryMatch> matches;
    for (const RegistrySnapshot& snapshot : snapshots)
    {
        if (snapshot.layoutCode != layoutCode)
        {
            continue;
        }

        RegistryMatch match;
        match.location = snapshot.location;
        match.valueName = snapshot.valueName;
        match.valueData = snapshot.valueData;
        matches.push_back(match);
    }

    return matches;
}

std::vector<std::string> RegistryService::listLayoutCodesFromRegistry() const
{
    const std::vector<RegistrySnapshot> snapshots = readRegistrySnapshots();
    std::unordered_set<std::string> uniqueLayouts;
    for (const RegistrySnapshot& snapshot : snapshots)
    {
        uniqueLayouts.insert(snapshot.layoutCode);
    }

    return std::vector<std::string>(uniqueLayouts.begin(), uniqueLayouts.end());
}

} // namespace ghost::platform
