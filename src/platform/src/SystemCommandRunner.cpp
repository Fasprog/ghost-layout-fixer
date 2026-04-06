#include <src/platform/SystemCommandRunner.h>

#include <array>
#include <cstdio>

namespace ghost::platform
{

CommandResult SystemCommandRunner::run(const std::string& command) const
{
    CommandResult result;

#if defined(_WIN32)
    FILE* pipe = _popen((command + " 2>&1").c_str(), "r");
#else
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
#endif

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

#if defined(_WIN32)
    result.exitCode = _pclose(pipe);
#else
    result.exitCode = pclose(pipe);
#endif

    return result;
}

} // namespace ghost::platform
