#pragma once

namespace ghost::common
{

/// @brief Универсальный контейнер результата выполнения.
template <typename T, typename E>
class Result
{
public:
    /// @brief Создаёт успешный результат.
    static Result success(const T& value)
    {
        return Result(value);
    }

    /// @brief Создаёт результат с ошибкой.
    static Result failure(const E& error)
    {
        return Result(error);
    }

    /// @brief Возвращает признак успешного выполнения.
    bool isSuccess() const
    {
        return success_;
    }

private:
    explicit Result(const T& value)
        : success_(true)
        , value_(value)
    {
    }

    explicit Result(const E& error)
        : success_(false)
        , error_(error)
    {
    }

    bool success_ = false;
    T value_ = {};
    E error_ = {};
};

} // namespace ghost::common
