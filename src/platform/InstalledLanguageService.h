#pragma once

#include <string>
#include <vector>

namespace ghost::platform
{

/// @brief Возвращает список установленных языков/раскладок Windows.
class InstalledLanguageService
{
public:
    /// @brief Получает коды установленных раскладок из системы.
    /// @return Список кодов раскладок (например, en-US, ru-RU).
    std::vector<std::string> listInstalledLayoutCodes() const;
};

} // namespace ghost::platform
