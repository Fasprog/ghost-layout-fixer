#pragma once

namespace ghost::common {

template <typename T, typename E>
class Result {
public:
    static Result success(const T& value) {
        return Result(value);
    }

    static Result failure(const E& error) {
        return Result(error);
    }

    bool isSuccess() const {
        return success_;
    }

private:
    explicit Result(const T& value) : success_(true), value_(value) {}
    explicit Result(const E& error) : success_(false), error_(error) {}

    bool success_{false};
    T value_{};
    E error_{};
};

} // namespace ghost::common
