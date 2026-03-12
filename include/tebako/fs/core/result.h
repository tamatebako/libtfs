/**
 * @file result.h
 * @brief Result type for structured error handling
 *
 * Provides a Result<T, E> type similar to Rust's Result or C++23's std::expected.
 * This enables functions to return either a value or an error without using
 * exceptions or output parameters.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <tebako/fs/core/error.h>

#include <variant>
#include <utility>
#include <type_traits>
#include <stdexcept>
#include <string_view>

namespace tebako {
namespace fs {

/**
 * @brief Exception thrown when unwrapping a Result that contains an error
 */
class BadResultAccess : public std::runtime_error {
 public:
  explicit BadResultAccess(const Error& err)
      : std::runtime_error("Attempted to access value of error Result: " + err.full_message())
  {
  }
};

/**
 * @brief Tag type for constructing an error Result
 *
 * @example
 * @code
 * auto result = Result<int>(Err{ErrorCode::NotFound, "Not found"});
 * @endcode
 */
struct Err {
  Error error;

  explicit Err(ErrorCode code) : error(code) {}
  explicit Err(Error err) : error(std::move(err)) {}
  Err(ErrorCode code, std::string message) : error(code, std::move(message)) {}
  Err(ErrorCode code, std::string message, std::string context) : error(code, std::move(message), std::move(context)) {}
  // String_view overloads for zero-copy context
  Err(ErrorCode code, std::string message, std::string_view context)
      : error(code, std::move(message), std::string(context))
  {
  }
};

/**
 * @brief Tag type for constructing a success Result
 *
 * @example
 * @code
 * auto result = Result<int>(Ok{42});
 * @endcode
 */
template <typename T>
struct Ok {
  T value;

  // Single constructor taking by value and moving
  explicit Ok(T v) : value(std::move(v)) {}
};

/**
 * @brief Specialization for void Ok
 */
template <>
struct Ok<void> {
  Ok() = default;
};

/**
 * @brief A type that can hold either a success value or an error
 *
 * Similar to Rust's Result<T, E> or C++23's std::expected<T, E>.
 * Provides a clean way to handle errors without exceptions.
 *
 * @tparam T The success value type
 *
 * @example
 * @code
 * Result<std::unique_ptr<FileHandle>> open(const std::string& path) {
 *     if (!exists(path)) {
 *         return Err{ErrorCode::NotFound, "File not found", path};
 *     }
 *     return Ok{std::make_unique<FileHandle>(path)};
 * }
 *
 * auto result = open("/path/to/file");
 * if (result.is_ok()) {
 *     auto handle = result.unwrap();
 *     // use handle
 * } else {
 *     std::cerr << result.error().message << std::endl;
 * }
 * @endcode
 */
template <typename T>
class Result {
 public:
  using value_type = T;
  using error_type = Error;

 private:
  std::variant<T, Error> data_;

 public:
  /**
   * @brief Default construct a Result (contains default T)
   */
  Result() : data_(T{}) {}

  /**
   * @brief Construct a success Result with a value
   */
  Result(T value) : data_(std::move(value)) {}  // NOLINT(google-explicit-constructor)

  /**
   * @brief Construct an error Result
   */
  Result(Err err) : data_(std::move(err.error)) {}  // NOLINT(google-explicit-constructor)

  /**
   * @brief Construct from Ok tag
   */
  Result(Ok<T> ok) : data_(std::move(ok.value)) {}  // NOLINT(google-explicit-constructor)

  /**
   * @brief Check if this Result contains a value (success)
   * @return true if success
   */
  [[nodiscard]] bool is_ok() const { return std::holds_alternative<T>(data_); }

  /**
   * @brief Check if this Result contains an error
   * @return true if error
   */
  [[nodiscard]] bool is_err() const { return std::holds_alternative<Error>(data_); }

  /**
   * @brief Boolean conversion (true if success)
   */
  explicit operator bool() const { return is_ok(); }

  /**
   * @brief Get the contained value (unchecked)
   *
   * WARNING: Undefined behavior if this Result contains an error.
   * Use is_ok() first or unwrap() for checked access.
   */
  [[nodiscard]] T& value() & { return std::get<T>(data_); }

  [[nodiscard]] const T& value() const& { return std::get<T>(data_); }

  [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }

  [[nodiscard]] const T&& value() const&& { return std::get<T>(std::move(data_)); }

  /**
   * @brief Get the contained error (unchecked)
   *
   * WARNING: Undefined behavior if this Result contains a value.
   */
  [[nodiscard]] Error& error() & { return std::get<Error>(data_); }

  [[nodiscard]] const Error& error() const& { return std::get<Error>(data_); }

  /**
   * @brief Get the contained value, throwing if error
   * @return Reference to the value
   * @throws BadResultAccess if Result contains an error
   */
  T& unwrap() &
  {
    if (is_err()) {
      throw BadResultAccess(error());
    }
    return value();
  }

  const T& unwrap() const&
  {
    if (is_err()) {
      throw BadResultAccess(error());
    }
    return value();
  }

  T&& unwrap() &&
  {
    if (is_err()) {
      throw BadResultAccess(error());
    }
    return std::move(value());
  }

  /**
   * @brief Get the contained value or a default
   * @param default_value Value to return if this Result contains an error
   * @return The value or default_value
   */
  template <typename U = T>
  T unwrap_or(U&& default_value) const&
  {
    return is_ok() ? value() : T(std::forward<U>(default_value));
  }

  template <typename U = T>
  T unwrap_or(U&& default_value) &&
  {
    return is_ok() ? std::move(value()) : T(std::forward<U>(default_value));
  }

  /**
   * @brief Get the contained value or compute from error
   * @param f Function that takes Error and returns T
   * @return The value or result of f(error)
   */
  template <typename F>
  T unwrap_or_else(F&& f) const&
  {
    return is_ok() ? value() : std::forward<F>(f)(error());
  }

  template <typename F>
  T unwrap_or_else(F&& f) &&
  {
    return is_ok() ? std::move(value()) : std::forward<F>(f)(error());
  }

  /**
   * @brief Map the value if success
   * @param f Function that takes T and returns U
   * @return Result<U> with mapped value or same error
   */
  template <typename F>
  auto map(F&& f) const& -> Result<std::invoke_result_t<F, const T&>>
  {
    using U = std::invoke_result_t<F, const T&>;
    if (is_ok()) {
      return Ok<U>{std::forward<F>(f)(value())};
    }
    return Err{error()};
  }

  template <typename F>
  auto map(F&& f) && -> Result<std::invoke_result_t<F, T&&>>
  {
    using U = std::invoke_result_t<F, T&&>;
    if (is_ok()) {
      return Ok<U>{std::forward<F>(f)(std::move(value()))};
    }
    return Err{error()};
  }

  /**
   * @brief Map the error if error
   * @param f Function that takes Error and returns Error
   * @return Result with same value or mapped error
   */
  template <typename F>
  Result<T> map_err(F&& f) &&
  {
    if (is_err()) {
      return Err{std::forward<F>(f)(error())};
    }
    return Ok<T>{std::move(value())};
  }

  /**
   * @brief Chain operations that return Result
   * @param f Function that takes T and returns Result<U>
   * @return Result from f or same error
   */
  template <typename F>
  auto and_then(F&& f) & -> std::invoke_result_t<F, T&>
  {
    using ResultType = std::invoke_result_t<F, T&>;
    if (is_ok()) {
      return std::forward<F>(f)(value());
    }
    return Err{error()};
  }

  template <typename F>
  auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&>
  {
    using ResultType = std::invoke_result_t<F, const T&>;
    if (is_ok()) {
      return std::forward<F>(f)(value());
    }
    return Err{error()};
  }

  template <typename F>
  auto and_then(F&& f) && -> std::invoke_result_t<F, T&&>
  {
    using ResultType = std::invoke_result_t<F, T&&>;
    if (is_ok()) {
      return std::forward<F>(f)(std::move(value()));
    }
    return Err{error()};
  }

  /**
   * @brief Compare two Results for equality
   */
  bool operator==(const Result& other) const { return data_ == other.data_; }

  bool operator!=(const Result& other) const { return data_ != other.data_; }
};

/**
 * @brief Specialization for void Result
 *
 * Used when an operation can fail but doesn't return a value.
 */
template <>
class Result<void> {
 public:
  using value_type = void;
  using error_type = Error;

 private:
  std::variant<std::monostate, Error> data_;

 public:
  /**
   * @brief Construct a success Result
   */
  Result() : data_(std::monostate{}) {}

  /**
   * @brief Construct from Ok<void> tag
   */
  Result(Ok<void>) : data_(std::monostate{}) {}  // NOLINT(google-explicit-constructor)

  /**
   * @brief Construct an error Result
   */
  Result(Err err) : data_(std::move(err.error)) {}  // NOLINT(google-explicit-constructor)

  /**
   * @brief Check if this Result is success
   */
  [[nodiscard]] bool is_ok() const { return std::holds_alternative<std::monostate>(data_); }

  /**
   * @brief Check if this Result is error
   */
  [[nodiscard]] bool is_err() const { return std::holds_alternative<Error>(data_); }

  /**
   * @brief Boolean conversion (true if success)
   */
  explicit operator bool() const { return is_ok(); }

  /**
   * @brief Get the error (unchecked)
   */
  [[nodiscard]] const Error& error() const { return std::get<Error>(data_); }

  /**
   * @brief Assert success, throw if error
   * @throws BadResultAccess if error
   */
  void unwrap() const
  {
    if (is_err()) {
      throw BadResultAccess(error());
    }
  }

  /**
   * @brief Map the error
   */
  template <typename F>
  Result<void> map_err(F&& f) &&
  {
    if (is_err()) {
      return Err{std::forward<F>(f)(error())};
    }
    return Ok<void>{};
  }

  bool operator==(const Result& other) const { return data_ == other.data_; }

  bool operator!=(const Result& other) const { return data_ != other.data_; }
};

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Create a success Result
 */
template <typename T>
Result<std::decay_t<T>> make_result(T&& value)
{
  return Result<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Create an error Result
 */
inline Result<void> make_error(ErrorCode code)
{
  return Err{code};
}

inline Result<void> make_error(ErrorCode code, std::string message)
{
  return Err{code, std::move(message)};
}

inline Result<void> make_error(ErrorCode code, std::string message, std::string context)
{
  return Err{code, std::move(message), std::move(context)};
}

/**
 * @brief Create a void success Result
 */
inline Result<void> make_ok()
{
  return Result<void>{};
}

}  // namespace fs
}  // namespace tebako
