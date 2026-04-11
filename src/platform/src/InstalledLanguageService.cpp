#include <src/platform/InstalledLanguageService.h>

#include <src/platform/SystemCommandRunner.h>

#include <sstream>
#include <unordered_set>

namespace
{

const ghost::platform::ICommandRunner& defaultRunner()
{
    static const ghost::platform::SystemCommandRunner runner;
    return runner;
}

std::vector<std::string> parseLanguageTags(const std::string& output)
{
    std::unordered_set<std::string> uniqueTags;
    std::vector<std::string> tags;
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
            if (uniqueTags.insert(tag).second)
            {
                tags.push_back(tag);
            }
        }
    }

    return tags;
}

} // namespace

namespace ghost::platform
{

InstalledLanguageService::InstalledLanguageService(const ICommandRunner* runner)
    : runner_(runner != nullptr ? runner : &defaultRunner())
{
}

std::vector<std::string> InstalledLanguageService::listInstalledLayoutCodes() const
{
    const CommandResult result = runner_->run(
        "powershell -NoProfile -Command \"(Get-WinUserLanguageList).LanguageTag\"");

    return parseLanguageTags(result.outputText);
}

} // namespace ghost::platform
