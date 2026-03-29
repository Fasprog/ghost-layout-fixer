#pragma once

#include <src/core/Models.h>

namespace ghost::core
{

/// @brief Координирует сценарии scan/fix/backup/restore.
class LayoutFixService
{
public:
    /// @brief Выполняет базовое сканирование состояния.
    /// @return Пустой результат сканирования на этапе скелета.
    ScanResult scan() const;
};

} // namespace ghost::core
