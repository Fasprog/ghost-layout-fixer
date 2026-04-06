#pragma once

#include <src/platform/SystemCommandRunner.h>

#include <string>
#include <vector>

namespace ghost::platform
{

/// @brief Совпадение записи реестра с целевой раскладкой.
struct RegistryMatch
{
    std::string location;
    std::string branchPath;
    std::string valueName;
    std::string valueData;
};

/// @brief Сервис доступа к whitelist-веткам реестра.
class RegistryService
{
public:
    explicit RegistryService(const ICommandRunner* runner = nullptr);

    /// @brief Ищет совпадения по коду раскладки.
    /// @param[in] layoutCode Код раскладки (например, en-GB).
    /// @return Список совпадений в реестре.
    std::vector<RegistryMatch> findLayoutMatches(const std::string& layoutCode) const;

    /// @brief Получает все коды раскладок, найденные в реестре.
    /// @return Список кодов раскладок из whitelist-веток.
    std::vector<std::string> listLayoutCodesFromRegistry() const;

    /// @brief Удаляет найденные совпадения по значениям в реестре.
    /// @param[in] matches Совпадения, которые требуется удалить.
    /// @return Ошибки удаления (пусто при полном успехе).
    std::vector<std::string> deleteMatches(const std::vector<RegistryMatch>& matches) const;

private:
    const ICommandRunner* runner_;
};

} // namespace ghost::platform
