#include <src/platform/InstalledLanguageService.h>

#include <src/platform/SystemCommandRunner.h>

#include <cstdlib>
#include <sstream>
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

std::vector<std::string> parseLanguageTags(const std::string& output)
{
    std::unordered_set<std::string> uniqueTags;
    std::stringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        std::string tag;
        for (const char symbol : line)
        {
            if (symbol != ' ' && symbol != '\t' && symbol != '\r')
            {
                tag.push_back(symbol);
            }
        }

        if (!tag.empty())
        {
            uniqueTags.insert(tag);
        }
    }

    return std::vector<std::string>(uniqueTags.begin(), uniqueTags.end());
}

} // namespace

namespace ghost::platform
{

std::vector<std::string> InstalledLanguageService::listInstalledLayoutCodes() const
{
    const char* mockLayouts = std::getenv("GLF_MOCK_INSTALLED_LAYOUTS");
    if (mockLayouts != nullptr)
    {
        return splitCsv(mockLayouts);
    }

    const SystemCommandRunner runner;
    const CommandResult result = runner.run(
        "powershell -NoProfile -Command \"(Get-WinUserLanguageList).LanguageTag\"");

    return parseLanguageTags(result.stdoutText);
}

} // namespace ghost::platform
