#include <src/cli/CliParser.h>

#include <string_view>

namespace ghost::cli
{

CliOptions CliParser::parse(int argc, const char* const argv[]) const
{
    CliOptions options;
    if (argc < 2)
    {
        options.showHelp = true;
        return options;
    }

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view arg{argv[index]};
        if (arg == "--help" || arg == "-h")
        {
            options.showHelp = true;
            return options;
        }
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
    else
    {
        options.parseErrors.push_back("unknown command: " + std::string(command));
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
        if (arg == "--layout")
        {
            options.parseErrors.push_back("missing value for --layout");
            continue;
        }

        if (arg == "--dry-run")
        {
            options.dryRun = true;
            continue;
        }

        if (arg == "--file" && index + 1 < argc)
        {
            options.restoreFile = argv[index + 1];
            ++index;
            continue;
        }
        if (arg == "--file")
        {
            options.parseErrors.push_back("missing value for --file");
            continue;
        }

        options.parseErrors.push_back("unknown argument: " + std::string(arg));
    }

    if (options.command == CommandType::Scan || options.command == CommandType::Backup)
    {
        if (options.layoutCode.has_value())
        {
            options.parseErrors.push_back("--layout is only supported for fix");
        }
        if (options.restoreFile.has_value())
        {
            options.parseErrors.push_back("--file is only supported for restore");
        }
        if (options.dryRun)
        {
            options.parseErrors.push_back("--dry-run is only supported for fix");
        }
    }

    if (options.command == CommandType::Fix && options.restoreFile.has_value())
    {
        options.parseErrors.push_back("--file cannot be used with fix");
    }

    if (options.command == CommandType::Restore)
    {
        if (options.layoutCode.has_value())
        {
            options.parseErrors.push_back("--layout cannot be used with restore");
        }
        if (options.dryRun)
        {
            options.parseErrors.push_back("--dry-run cannot be used with restore");
        }
    }

    return options;
}

} // namespace ghost::cli
