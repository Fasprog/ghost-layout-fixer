#pragma once

#include <string>

namespace ghost::common {

class Logger {
public:
    void info(const std::string& message) const;
};

} // namespace ghost::common
