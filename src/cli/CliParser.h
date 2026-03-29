#pragma once

#include <src/cli/CliTypes.h>

namespace ghost::cli
{

/// @brief Разбирает аргументы командной строки в структуру опций.
class CliParser
{
public:
    /// @brief Парсит входные аргументы процесса.
    /// @param[in] argc Количество аргументов.
    /// @param[in] argv Массив аргументов.
    /// @return Структура распознанных CLI-опций.
    CliOptions parse(int argc, const char* const argv[]) const;
};

} // namespace ghost::cli
