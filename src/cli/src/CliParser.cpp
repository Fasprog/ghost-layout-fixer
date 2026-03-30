#include <src/cli/CliParser.h>

#include <string_view>

namespace ghost::cli
{

CliOptions CliParser::parse(int argc, const char* const argv[]) const
{
    CliOptions options;
    if (argc < 2)
    {
        return options;
    }

    std::string_view command{argv[1]};
    if (command == "scan")
    {
        options.command = CommandType::Scan;
    }
    else if (command == "fix")
    {
        options.command = CommandType::Fix;
    }
    else if (command == "backup")
    {
        options.command = CommandType::Backup;
    }
    else if (command == "restore")
    {
        options.command = CommandType::Restore;
    }

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view arg{argv[index]};
        if (arg == "--layout" && index + 1 < argc)
        {
            options.layoutCode = argv[index + 1];
            ++index;
            continue;
        }

        if (arg == "--dry-run")
        {
            options.dryRun = true;
            continue;
        }

        if (arg == "--yes")
        {
            options.assumeYes = true;
            continue;
        }

        if (arg == "--file" && index + 1 < argc)
        {
            options.restoreFile = argv[index + 1];
            ++index;
            continue;
        }
    }

    return options;
}

} // namespace ghost::cli
