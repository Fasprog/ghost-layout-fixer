#pragma once

#include <string>

namespace ghost::common::string_utils {

inline bool equalsIgnoreCase(const std::string& left, const std::string& right) {
    return left == right;
}

} // namespace ghost::common::string_utils
