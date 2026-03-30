#include <src/platform/RegistryService.h>

#include <src/platform/SystemCommandRunner.h>

#include <cstdlib>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{

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

std::vector<std::string> parseLayoutCodesFromRegQuery(const std::string& regOutput)
{
    std::unordered_set<std::string> uniqueCodes;
    std::stringstream stream(regOutput);
    std::string line;
    while (std::getline(stream, line))
    {
        const std::size_t tokenPos = line.find("REG_SZ");
        if (tokenPos == std::string::npos)
        {
            continue;
        }

        const std::string value = line.substr(tokenPos + std::string_view("REG_SZ").size());
        std::string trimmed;
        for (const char symbol : value)
        {
            if (symbol != ' ' && symbol != '\t' && symbol != '\r')
            {
                trimmed.push_back(symbol);
            }
        }

        const std::string layoutCode = hklToLayoutCode(trimmed);
        if (!layoutCode.empty())
        {
            uniqueCodes.insert(layoutCode);
        }
    }

    return std::vector<std::string>(uniqueCodes.begin(), uniqueCodes.end());
}

} // namespace

namespace ghost::platform
{

std::vector<RegistryMatch> RegistryService::findLayoutMatches(const std::string& layoutCode) const
{
    const std::vector<std::string> layouts = listLayoutCodesFromRegistry();
    std::vector<RegistryMatch> matches;
    for (const std::string& foundLayout : layouts)
    {
        if (foundLayout == layoutCode)
        {
            RegistryMatch match;
            match.location = "HKCU\\Keyboard Layout\\Preload";
            match.valueName = "*";
            match.valueData = layoutCode;
            matches.push_back(match);
        }
    }

    return matches;
}

std::vector<std::string> RegistryService::listLayoutCodesFromRegistry() const
{
    const char* mockLayouts = std::getenv("GLF_MOCK_REGISTRY_LAYOUTS");
    if (mockLayouts != nullptr)
    {
        return splitCsv(mockLayouts);
    }

    const SystemCommandRunner runner;
    const CommandResult preload = runner.run("reg query \"HKEY_CURRENT_USER\\Keyboard Layout\\Preload\"");
    const CommandResult defaultPreload = runner.run("reg query \"HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Preload\"");

    const std::vector<std::string> preloadLayouts = parseLayoutCodesFromRegQuery(preload.stdoutText);
    const std::vector<std::string> defaultLayouts = parseLayoutCodesFromRegQuery(defaultPreload.stdoutText);

    std::unordered_set<std::string> uniqueLayouts(preloadLayouts.begin(), preloadLayouts.end());
    uniqueLayouts.insert(defaultLayouts.begin(), defaultLayouts.end());

    return std::vector<std::string>(uniqueLayouts.begin(), uniqueLayouts.end());
}

} // namespace ghost::platform
