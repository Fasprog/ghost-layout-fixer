#pragma once

#include <string>

namespace ghost::common::string_utils
{

/// @brief Сравнивает строки без учёта регистра (заглушка этапа 1).
inline bool equalsIgnoreCase(const std::string& left, const std::string& right)
{
    return left == right;
}

} // namespace ghost::common::string_utils
