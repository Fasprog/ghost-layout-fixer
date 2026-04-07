#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ghost::cli
{

/// @brief Тип поддерживаемой CLI-команды.
enum class CommandType
{
    Scan,
    Fix,
    Backup,
    Restore,
    Unknown
};

/// @brief Опции командной строки, полученные после парсинга.
struct CliOptions
{
    CommandType command = CommandType::Unknown;
    std::optional<std::string> layoutCode;
    bool dryRun = false;
    std::optional<std::string> restoreFile;
    bool assumeYes = false;
    bool showHelp = false;
    std::vector<std::string> parseErrors;
};

} // namespace ghost::cli
