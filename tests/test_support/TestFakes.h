#pragma once

#include <src/platform/PrivilegeService.h>
#include <src/platform/SystemCommandRunner.h>

#include <fstream>
#include <string>
#include <vector>

namespace ghost::tests
{

struct FakePrivilegeService final : ghost::platform::IPrivilegeService
{
    bool isAdmin{true};
    bool isRunningAsAdmin() const override { return isAdmin; }
};

struct FakeCommandRunner final : ghost::platform::ICommandRunner
{
    struct Rule
    {
        std::string contains;
        int exitCode{0};
        std::string output;
    };

    mutable std::vector<std::string> commands;
    std::vector<Rule> rules;

    static std::string extractQuoted(const std::string& command, int quoteIndex)
    {
        int current = -1;
        std::size_t segmentIndex = 0;
        for (std::size_t i = 0; i < command.size(); ++i)
        {
            if (command[i] == '"')
            {
                if (current == -1)
                {
                    current = static_cast<int>(i);
                    continue;
                }

                if (segmentIndex == static_cast<std::size_t>(quoteIndex))
                {
                    return command.substr(static_cast<std::size_t>(current + 1), i - static_cast<std::size_t>(current + 1));
                }
                ++segmentIndex;
                current = -1;
            }
        }

        return {};
    }

    ghost::platform::CommandResult run(const std::string& command) const override
    {
        commands.push_back(command);
        for (const Rule& rule : rules)
        {
            if (command.find(rule.contains) == std::string::npos)
            {
                continue;
            }

            if (rule.exitCode == 0 && command.find("reg export") != std::string::npos)
            {
                const std::string branch = extractQuoted(command, 0);
                const std::string file = extractQuoted(command, 1);
                std::ofstream out(file, std::ios::binary | std::ios::trunc);
                const std::u16string payload = u"\uFEFFWindows Registry Editor Version 5.00\r\n\r\n[" +
                                               std::u16string(branch.begin(), branch.end()) +
                                               u"]\r\n\"1\"=\"00000409\"\r\n";
                out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size() * sizeof(char16_t)));
            }

            return ghost::platform::CommandResult{rule.exitCode, rule.output};
        }

        return ghost::platform::CommandResult{};
    }
};

} // namespace ghost::tests
