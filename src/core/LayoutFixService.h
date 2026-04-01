#pragma once

#include <src/core/Models.h>

#include <string>
#include <vector>

namespace ghost::core
{

/// @brief Координирует сценарии scan/fix/backup/restore.
class LayoutFixService
{
public:
    /// @brief Выявляет призрачные раскладки из разницы реестра и установленных языков.
    /// При сравнении учитывает нормализацию тегов (`ru` ~= `ru-RU`).
    /// @param[in] registryLayouts Коды раскладок, найденные в реестре.
    /// @param[in] installedLayouts Коды реально установленных раскладок.
    /// @return Результат сканирования с перечнем ghost-раскладок.
    ScanResult scan(
        const std::vector<std::string>& registryLayouts,
        const std::vector<std::string>& installedLayouts) const;

    /// @brief Формирует безопасный план fix для режима --dry-run.
    FixPlan buildDryRunPlan(
        const std::string& layoutCode,
        const std::vector<std::string>& registryMatchSummaries,
        const std::string& backupPath) const;
};

} // namespace ghost::core
