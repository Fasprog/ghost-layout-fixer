#pragma once

#include <string>

namespace ghost::platform
{

/// @brief Результат запуска системной команды.
struct CommandResult
{
    int exitCode{0};
    /// @brief Объединённый stdout/stderr поток команды.
    std::string outputText;
};

/// @brief Контракт запуска системных команд.
class ICommandRunner
{
public:
    virtual ~ICommandRunner() = default;
    virtual CommandResult run(const std::string& command) const = 0;
};

/// @brief Запускает системные команды и возвращает результат.
class SystemCommandRunner final : public ICommandRunner
{
public:
    /// @brief Выполняет команду в системе.
    /// @param[in] command Строка команды.
    /// @return Результат выполнения команды.
    CommandResult run(const std::string& command) const override;
};

} // namespace ghost::platform
