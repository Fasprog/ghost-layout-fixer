#pragma once

#include <string>
#include <vector>

namespace ghost::platform
{

/// @brief Совпадение записи реестра с целевой раскладкой.
struct RegistryMatch
{
    std::string location;
    std::string valueName;
    std::string valueData;
};

/// @brief Сервис доступа к whitelist-веткам реестра.
class RegistryService
{
public:
    /// @brief Ищет совпадения по коду раскладки.
    /// @param[in] layoutCode Код раскладки (например, en-GB).
    /// @return Список совпадений в реестре.
    std::vector<RegistryMatch> findLayoutMatches(const std::string& layoutCode) const;

    /// @brief Получает все коды раскладок, найденные в реестре.
    /// @return Список кодов раскладок из whitelist-веток.
    std::vector<std::string> listLayoutCodesFromRegistry() const;
};

} // namespace ghost::platform
