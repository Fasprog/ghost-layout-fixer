#pragma once

#include <optional>
#include <string>

namespace ghost::cli {

enum class CommandType {
    Scan,
    Fix,
    Backup,
    Restore,
    Unknown
};

struct CliOptions {
    CommandType command{CommandType::Unknown};
    std::optional<std::string> layoutCode;
    bool dryRun{false};
    std::optional<std::string> restoreFile;
    bool assumeYes{false};
};

} // namespace ghost::cli
