#pragma once

namespace ghost::platform
{

/// @brief Контракт проверки прав текущего процесса.
class IPrivilegeService
{
public:
    virtual ~IPrivilegeService() = default;
    virtual bool isRunningAsAdmin() const = 0;
};

/// @brief Проверяет права текущего процесса.
class PrivilegeService final : public IPrivilegeService
{
public:
    /// @brief Проверяет, запущен ли процесс с правами администратора.
    /// @return true, если права администратора доступны.
    bool isRunningAsAdmin() const override;
};

} // namespace ghost::platform
