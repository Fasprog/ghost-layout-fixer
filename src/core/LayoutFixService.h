#pragma once

#include <src/core/Models.h>
#include <src/platform/RegistryService.h>
#include <src/platform/SystemCommandRunner.h>

#include <string>
#include <vector>

namespace ghost::core
{

/// @brief Координирует сценарии scan/fix/backup/restore.
class LayoutFixService
{
public:
    explicit LayoutFixService(const ghost::platform::ICommandRunner* runner = nullptr);

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

    /// @brief Выполняет реальный fix: add/remove cycle + cleanup следов в реестре.
    /// @param[in] layoutCode Целевая раскладка.
    /// @param[in] matches Совпадения, подлежащие удалению.
    /// @param[in] backupPath Путь к уже созданному backup.
    /// @param[in] registryService Сервис удаления совпадений.
    /// @return Итоговый отчёт фиксации.
    FixReport executeFix(
        const std::string& layoutCode,
        const std::vector<ghost::platform::RegistryMatch>& matches,
        const std::string& backupPath,
        const ghost::platform::RegistryService& registryService) const;

private:
    const ghost::platform::ICommandRunner* runner_;
};

} // namespace ghost::core
