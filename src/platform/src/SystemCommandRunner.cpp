#include <src/platform/SystemCommandRunner.h>

#include <array>
#include <cstdio>

namespace ghost::platform
{

CommandResult SystemCommandRunner::run(const std::string& command) const
{
    CommandResult result;

    FILE* pipe = _popen((command + " 2>&1").c_str(), "r");

    if (pipe == nullptr)
    {
        result.exitCode = 1;
        result.outputText = "failed to start command";
        return result;
    }

    std::array<char, 256> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
    {
        result.outputText += buffer.data();
    }

    result.exitCode = _pclose(pipe);

    return result;
}

} // namespace ghost::platform
