#pragma once

namespace ghost::platform
{

/// @brief Проверяет права текущего процесса.
class PrivilegeService
{
public:
    /// @brief Проверяет, запущен ли процесс с правами администратора.
    /// @return true, если права администратора доступны.
    bool isRunningAsAdmin() const;
};

} // namespace ghost::platform
