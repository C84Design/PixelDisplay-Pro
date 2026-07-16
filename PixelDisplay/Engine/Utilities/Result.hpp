// PixelDisplay Pro — Engine/Utilities/Result.hpp
//
// A lightweight expected<T, Error>-style type for the C++20 engine. The engine
// avoids exceptions across its public API so that host adapters (After Effects,
// Premiere, OpenFX) can translate failures into host-native error codes without
// unwinding through Adobe C code.
#pragma once

#include <string>
#include <utility>
#include <variant>

namespace pd {

/// Coarse error categories. Host adapters map these to host error codes.
enum class ErrorCode {
    None,
    InvalidArgument,
    OutOfMemory,
    BackendUnavailable,
    DeviceLost,
    UnsupportedFormat,
    ShaderCompileFailed,
    Internal,
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

/// Minimal Result<T>. Holds either a value or an Error. Never throws.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}

    bool ok() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return ok(); }

    T& value() & { return std::get<T>(storage_); }
    const T& value() const& { return std::get<T>(storage_); }
    T&& value() && { return std::get<T>(std::move(storage_)); }

    T value_or(T fallback) const {
        return ok() ? std::get<T>(storage_) : std::move(fallback);
    }

    const Error& error() const { return std::get<Error>(storage_); }

private:
    std::variant<T, Error> storage_;
};

/// Result specialization for operations that return only success/failure.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;                         // success
    Result(Error error) : error_(std::move(error)), hasError_(true) {}

    bool ok() const noexcept { return !hasError_; }
    explicit operator bool() const noexcept { return ok(); }
    const Error& error() const { return error_; }

private:
    Error error_{};
    bool hasError_ = false;
};

using Status = Result<void>;

}  // namespace pd
