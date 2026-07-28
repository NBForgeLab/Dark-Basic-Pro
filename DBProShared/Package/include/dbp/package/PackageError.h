#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace dbp::package {

enum class PackageErrorCode {
    UnexpectedEnd,
    ArithmeticOverflow,
    InvalidFormat,
    UnsupportedVersion,
    UnsupportedAlgorithm,
    LimitExceeded,
    UnsafePath,
    MissingKey,
    CryptographyFailed,
    AuthenticationFailed,
    IntegrityFailed,
    CompressionFailed,
    IoFailed,
    PublicationFailed,
};

struct PackageError {
    PackageErrorCode code;
    std::string message;
    std::optional<std::uint64_t> offset;
};

template <typename T>
class PackageResult {
public:
    static PackageResult Success(T value) {
        return PackageResult(std::move(value));
    }

    static PackageResult Failure(PackageError error) {
        return PackageResult(std::move(error));
    }

    bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    const T& value() const {
        return std::get<T>(storage_);
    }

    T& value() {
        return std::get<T>(storage_);
    }

    const PackageError& error() const {
        return std::get<PackageError>(storage_);
    }

private:
    explicit PackageResult(T value)
        : storage_(std::move(value)) {}

    explicit PackageResult(PackageError error)
        : storage_(std::move(error)) {}

    std::variant<T, PackageError> storage_;
};

} // namespace dbp::package
