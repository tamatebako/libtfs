/**
 * @file path.h
 * @brief Path value class for filesystem path handling
 *
 * Provides a clean, immutable path abstraction with normalization.
 * This centralizes path handling logic that was previously duplicated
 * across backends.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <string>
#include <string_view>
#include <optional>

namespace tebako {
namespace fs {

/**
 * @brief Immutable path value class with normalization
 *
 * This class provides a clean abstraction for filesystem paths with:
 * - Automatic normalization (removes redundant separators, handles . and ..)
 * - Common path operations (parent, filename, extension, join)
 * - Efficient string_view compatibility
 * - No dynamic allocation for simple operations
 *
 * @example
 * @code
 * Path p("/home/user/documents/file.txt");
 * p.is_absolute();           // true
 * p.filename();              // "file.txt"
 * p.extension();             // ".txt"
 * p.parent();                // Path("/home/user/documents")
 *
 * Path combined = Path("/home").join("user").join("file.txt");
 * Path relative = Path::relative_to("/home/user/file", "/home");
 * // relative = "user/file"
 * @endcode
 */
class Path {
private:
    std::string path_;

    // Normalize the path during construction
    static std::string normalize(std::string_view p);

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /**
     * @brief Default constructor creates empty path
     */
    Path() = default;

    /**
     * @brief Construct from string_view (implicit for convenience)
     * @param p Path string (automatically normalized)
     */
    Path(std::string_view p) : path_(normalize(p)) {}  // NOLINT(google-explicit-constructor)

    /**
     * @brief Construct from std::string (implicit for convenience)
     * @param p Path string (automatically normalized)
     */
    Path(const std::string& p) : path_(normalize(p)) {}  // NOLINT(google-explicit-constructor)

    /**
     * @brief Construct from C-string (implicit for convenience)
     * @param p Path string (automatically normalized)
     */
    Path(const char* p) : path_(normalize(p ? p : "")) {}  // NOLINT(google-explicit-constructor)

    // =========================================================================
    // Properties
    // =========================================================================

    /**
     * @brief Check if path is empty
     */
    [[nodiscard]] bool empty() const {
        return path_.empty();
    }

    /**
     * @brief Check if path is absolute (starts with '/')
     */
    [[nodiscard]] bool is_absolute() const {
        return !path_.empty() && path_.front() == '/';
    }

    /**
     * @brief Check if path is relative (does not start with '/')
     */
    [[nodiscard]] bool is_relative() const {
        return !path_.empty() && path_.front() != '/';
    }

    /**
     * @brief Check if this is the root path "/"
     */
    [[nodiscard]] bool is_root() const {
        return path_ == "/";
    }

    /**
     * @brief Get path length
     */
    [[nodiscard]] size_t length() const {
        return path_.length();
    }

    // =========================================================================
    // Component Extraction
    // =========================================================================

    /**
     * @brief Get the parent directory path
     * @return Parent path, or empty if no parent
     *
     * @example
     * Path("/home/user/file.txt").parent() -> Path("/home/user")
     * Path("file.txt").parent() -> Path()
     * Path("/").parent() -> Path()
     */
    [[nodiscard]] Path parent() const;

    /**
     * @brief Get the filename component (last segment)
     * @return Filename, or empty if no filename
     *
     * @example
     * Path("/home/user/file.txt").filename() -> "file.txt"
     * Path("/home/user/").filename() -> "user"
     * Path("/").filename() -> ""
     */
    [[nodiscard]] std::string filename() const;

    /**
     * @brief Get the file extension (including dot)
     * @return Extension including dot, or empty if no extension
     *
     * @example
     * Path("/home/file.txt").extension() -> ".txt"
     * Path("/home/file").extension() -> ""
     * Path("/home/.hidden").extension() -> ""
     */
    [[nodiscard]] std::string extension() const;

    /**
     * @brief Get the stem (filename without extension)
     * @return Stem, or empty if no filename
     *
     * @example
     * Path("/home/file.txt").stem() -> "file"
     * Path("/home/file").stem() -> "file"
     */
    [[nodiscard]] std::string stem() const;

    // =========================================================================
    // Path Operations
    // =========================================================================

    /**
     * @brief Join with another path component
     * @param other Path to join
     * @return Combined path
     *
     * @example
     * Path("/home").join("user") -> Path("/home/user")
     * Path("/home/").join("/user") -> Path("/home/user")
     */
    [[nodiscard]] Path join(const Path& other) const;

    /**
     * @brief Get path relative to a base
     * @param base Base path to make relative to
     * @return Relative path, or empty if not under base
     *
     * @example
     * Path::relative_to("/home/user/file", "/home") -> "user/file"
     * Path::relative_to("/home/user/file", "/other") -> ""
     */
    [[nodiscard]] static Path relative_to(const Path& path, const Path& base);

    // =========================================================================
    // String Access
    // =========================================================================

    /**
     * @brief Get path as string
     */
    [[nodiscard]] const std::string& string() const& {
        return path_;
    }

    /**
     * @brief Get path as string (rvalue)
     */
    [[nodiscard]] std::string string() && {
        return std::move(path_);
    }

    /**
     * @brief Get path as C-string
     */
    [[nodiscard]] const char* c_str() const {
        return path_.c_str();
    }

    /**
     * @brief Get path as string_view
     */
    [[nodiscard]] std::string_view view() const {
        return path_;
    }

    /**
     * @brief Implicit conversion to string_view
     */
    operator std::string_view() const {
        return path_;
    }

    // =========================================================================
    // Comparison
    // =========================================================================

    bool operator==(const Path& other) const {
        return path_ == other.path_;
    }

    bool operator!=(const Path& other) const {
        return path_ != other.path_;
    }

    bool operator<(const Path& other) const {
        return path_ < other.path_;
    }

    // =========================================================================
    // Backend-Specific Utilities
    // =========================================================================

    /**
     * @brief Get path without leading slash (for archive entry lookup)
     * @return Path without leading slash
     *
     * @example
     * Path("/home/user/file").without_leading_slash() -> "home/user/file"
     * Path("home/user/file").without_leading_slash() -> "home/user/file"
     */
    [[nodiscard]] std::string without_leading_slash() const;

    /**
     * @brief Get path with trailing slash (for directory operations)
     * @return Path with trailing slash (or "/" for root)
     *
     * @example
     * Path("/home/user").with_trailing_slash() -> "/home/user/"
     * Path("/").with_trailing_slash() -> "/"
     */
    [[nodiscard]] std::string with_trailing_slash() const;

    /**
     * @brief Check if this path has a specific extension (case-insensitive)
     * @param ext Extension to check (with or without dot)
     *
     * @example
     * Path("file.txt").has_extension(".txt") -> true
     * Path("file.TXT").has_extension("txt") -> true
     */
    [[nodiscard]] bool has_extension(std::string_view ext) const;
};

} // namespace fs
} // namespace tebako
