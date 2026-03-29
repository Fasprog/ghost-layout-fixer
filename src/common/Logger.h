#pragma once

#include <string>

namespace ghost::common
{

/// @brief Логгер для консольного вывода на этапе скелета.
class Logger
{
public:
    /// @brief Выводит информационное сообщение.
    /// @param[in] message Текст сообщения.
    void info(const std::string& message) const;
};

} // namespace ghost::common
