#pragma once

#include <string>

namespace ghost::platform
{

/// @brief Результат запуска системной команды.
struct CommandResult
{
    int exitCode{0};
    std::string stdoutText;
    std::string stderrText;
};

/// @brief Запускает системные команды и возвращает результат.
class SystemCommandRunner
{
public:
    /// @brief Выполняет команду в системе.
    /// @param[in] command Строка команды.
    /// @return Результат выполнения команды.
    CommandResult run(const std::string& command) const;
};

} // namespace ghost::platform
