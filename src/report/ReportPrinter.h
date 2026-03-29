#pragma once

#include <string>

namespace ghost::report {

class ReportPrinter {
public:
    void print(const std::string& message) const;
};

} // namespace ghost::report
