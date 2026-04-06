#pragma once

#include <src/platform/SystemCommandRunner.h>

#include <string>
#include <vector>

namespace ghost::platform
{

/// @brief Возвращает список установленных языков/раскладок Windows.
class InstalledLanguageService
{
public:
    explicit InstalledLanguageService(const ICommandRunner* runner = nullptr);

    /// @brief Получает коды установленных раскладок из системы.
    /// @return Список кодов раскладок (например, en-US, ru-RU).
    std::vector<std::string> listInstalledLayoutCodes() const;

private:
    const ICommandRunner* runner_;
};

} // namespace ghost::platform
