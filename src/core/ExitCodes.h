#pragma once

namespace ghost::core {

enum class ExitCode {
    Success = 0,
    GeneralError = 1,
    InsufficientPrivileges = 2,
    BackupError = 3,
    FixError = 4,
    RestoreError = 5
};

} // namespace ghost::core
