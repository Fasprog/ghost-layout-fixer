#include "cli/CliParser.h"

#include <string_view>

namespace ghost::cli {

CliOptions CliParser::parse(int argc, const char* const argv[]) const {
    CliOptions options;
    if (argc < 2) {
        return options;
    }

    std::string_view command{argv[1]};
    if (command == "scan") {
        options.command = CommandType::Scan;
    } else if (command == "fix") {
        options.command = CommandType::Fix;
    } else if (command == "backup") {
        options.command = CommandType::Backup;
    } else if (command == "restore") {
        options.command = CommandType::Restore;
    }

    return options;
}

} // namespace ghost::cli
