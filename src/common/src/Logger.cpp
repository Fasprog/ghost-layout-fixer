#include <src/common/Logger.h>

#include <iostream>

namespace ghost::common
{

void Logger::info(const std::string& message) const
{
    std::cout << "[INFO] " << message << '\n';
}

} // namespace ghost::common
