#pragma once

#include <string>

namespace ghost::platform {

struct CommandResult {
    int exitCode{0};
    std::string stdoutText;
    std::string stderrText;
};

class SystemCommandRunner {
public:
    CommandResult run(const std::string& command) const;
};

} // namespace ghost::platform
