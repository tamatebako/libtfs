/**
 * @file error.h
 * @brief Error handling types for libtfs
 *
 * Provides structured error information with machine-readable codes
 * and human-readable messages.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <string>
#include <cstdint>

namespace tebako {
namespace fs {

/**
 * @brief Machine-readable error codes
 *
 * These codes can be used for programmatic error handling and
 * are stable across library versions.
 */
enum class ErrorCode : int {
    /** Operation completed successfully */
    Success = 0,

    /** File or path not found */
    NotFound = 1,

    /** Path exists but is not a directory */
    NotADirectory = 2,

    /** Path exists but is not a regular file */
    NotAFile = 3,

    /** Permission denied */
    PermissionDenied = 4,

    /** Invalid argument provided */
    InvalidArgument = 5,

    /** No filesystem is currently mounted */
    NotMounted = 6,

    /** A filesystem is already mounted */
    AlreadyMounted = 7,

    /** Archive file is corrupted or invalid */
    CorruptedArchive = 8,

    /** Memory allocation failed */
    OutOfMemory = 9,

    /** General I/O error */
    IOError = 10,

    /** Operation not supported by this backend */
    NotSupported = 11,

    /** File or directory already exists */
    AlreadyExists = 12,

    /** File handle is invalid or closed */
    BadFileDescriptor = 13,

    /** Directory is not empty */
    DirectoryNotEmpty = 14,

    /** Cross-device link not permitted */
    CrossDeviceLink = 15,

    /** Name too long */
    NameTooLong = 16,

    /** Too many symbolic links */
    TooManySymlinks = 17,

    /** Unknown error */
    Unknown = 99
};

/**
 * @brief Structured error information
 *
 * Contains both machine-readable error code and human-readable
 * message with optional context information.
 *
 * @example
 * @code
 * Error err(ErrorCode::NotFound, "File not found", "/path/to/file.txt");
 * if (err) {
 *     std::cerr << err.message << ": " << err.context << std::endl;
 * }
 * @endcode
 */
struct Error {
    /** Machine-readable error code */
    ErrorCode code = ErrorCode::Success;

    /** Human-readable error message */
    std::string message;

    /** Additional context (e.g., file path) */
    std::string context;

    /**
     * @brief Default constructor creates a success (no error) state
     */
    Error() = default;

    /**
     * @brief Construct an error with code only
     * @param c Error code
     */
    explicit Error(ErrorCode c) : code(c), message(error_to_string(c)) {}

    /**
     * @brief Construct an error with code and message
     * @param c Error code
     * @param msg Human-readable message
     */
    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}

    /**
     * @brief Construct an error with code, message, and context
     * @param c Error code
     * @param msg Human-readable message
     * @param ctx Additional context (e.g., file path)
     */
    Error(ErrorCode c, std::string msg, std::string ctx)
        : code(c), message(std::move(msg)), context(std::move(ctx)) {}

    /**
     * @brief Check if this represents an error (non-success)
     * @return true if code is not Success
     */
    explicit operator bool() const {
        return code != ErrorCode::Success;
    }

    /**
     * @brief Check if this represents success
     * @return true if code is Success
     */
    bool is_ok() const {
        return code == ErrorCode::Success;
    }

    /**
     * @brief Check if this represents an error
     * @return true if code is not Success
     */
    bool is_err() const {
        return code != ErrorCode::Success;
    }

    /**
     * @brief Compare two Errors for equality
     */
    bool operator==(const Error& other) const {
        return code == other.code && message == other.message && context == other.context;
    }

    bool operator!=(const Error& other) const {
        return !(*this == other);
    }

    /**
     * @brief Get full error description
     * @return Formatted string with message and context
     */
    std::string full_message() const {
        if (context.empty()) {
            return message;
        }
        return message + ": " + context;
    }

    /**
     * @brief Convert error code to string
     * @param code Error code to convert
     * @return String representation
     */
    static const char* error_to_string(ErrorCode code) {
        switch (code) {
            case ErrorCode::Success: return "Success";
            case ErrorCode::NotFound: return "Not found";
            case ErrorCode::NotADirectory: return "Not a directory";
            case ErrorCode::NotAFile: return "Not a file";
            case ErrorCode::PermissionDenied: return "Permission denied";
            case ErrorCode::InvalidArgument: return "Invalid argument";
            case ErrorCode::NotMounted: return "Not mounted";
            case ErrorCode::AlreadyMounted: return "Already mounted";
            case ErrorCode::CorruptedArchive: return "Corrupted archive";
            case ErrorCode::OutOfMemory: return "Out of memory";
            case ErrorCode::IOError: return "I/O error";
            case ErrorCode::NotSupported: return "Operation not supported";
            case ErrorCode::AlreadyExists: return "Already exists";
            case ErrorCode::BadFileDescriptor: return "Bad file descriptor";
            case ErrorCode::DirectoryNotEmpty: return "Directory not empty";
            case ErrorCode::CrossDeviceLink: return "Cross-device link";
            case ErrorCode::NameTooLong: return "Name too long";
            case ErrorCode::TooManySymlinks: return "Too many symbolic links";
            case ErrorCode::Unknown:
            default: return "Unknown error";
        }
    }
};

/**
 * @brief Create a NotFound error with path context
 * @param path The path that was not found
 * @return Error with NotFound code and path context
 */
inline Error make_not_found_error(const std::string& path) {
    return Error(ErrorCode::NotFound, "Path not found", path);
}

/**
 * @brief Create an IOError with description
 * @param description Description of the I/O error
 * @return Error with IOError code
 */
inline Error make_io_error(const std::string& description) {
    return Error(ErrorCode::IOError, "I/O error", description);
}

/**
 * @brief Create an InvalidArgument error
 * @param description Description of the invalid argument
 * @return Error with InvalidArgument code
 */
inline Error make_invalid_argument_error(const std::string& description) {
    return Error(ErrorCode::InvalidArgument, "Invalid argument", description);
}

} // namespace fs
} // namespace tebako
