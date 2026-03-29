#include "platform/SystemCommandRunner.h"

namespace ghost::platform {

CommandResult SystemCommandRunner::run(const std::string& command) const {
    (void)command;
    return CommandResult{};
}

} // namespace ghost::platform
