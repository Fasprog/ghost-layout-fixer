#pragma once

#include <string>

namespace ghost::report
{

/// @brief Печатает отчёты и диагностические сообщения в консоль.
class ReportPrinter
{
public:
    /// @brief Выводит сообщение в stdout.
    /// @param[in] message Текст сообщения.
    void print(const std::string& message) const;
};

} // namespace ghost::report
