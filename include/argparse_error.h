#ifndef ARGPARSE_ERROR_H
#define ARGPARSE_ERROR_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
    /**
     * @brief Categorizes all possible errors in the argparse library, each with specific semantics and severity.
    */
    typedef enum ArgParseErrorCategory ArgParseErrorCategory;

    /* @brief Complete error context structure containing all error information. */
    typedef struct ArgParseError ArgParseError;

    enum ArgParseErrorCategory {
        APE_SUCCESS = 0,      /* Operation completed successfully. */
        APE_MEMORY,           /* Memory allocation failure. */
        APE_SYNTAX,           /* Command line syntax error. */
        APE_TYPE,             /* Type conversion/parsing error. */
        APE_REQUIRED,         /* Required argument missing. */
        APE_VALIDATION,       /* Validation/constraint error. */
        APE_INTERNAL,         /* Internal library error. */
        APE_CONFIG,           /* Configuration/initialization error. */
        APE_RANGE,            /* Value out of valid range. */
        APE_UNKNOWN_ARG,      /* Unknown argument provided. */
        APE_DUPLICATE,        /* Duplicate argument definition. */
        APE_HELP_REQUESTED    /* Help requested (non-fatal). */
    };

    struct ArgParseError {
        ArgParseErrorCategory category;
        int errno_value;
        const char* function_name;
        int line_number;
        const char* argument_name;
        const char* user_message;
    };

    /**
     * @brief Records an error with full context in thread-local storage. 
     * @param category Error category classification
     * @param errno_val Standard C errno value to set
     * @param func Name of function where error occurred (use __func__)
     * @param line Line number where error occurred (use __LINE__)
     * @param arg_name Name of argument causing error (NULL if not applicable)
     * @param user_msg Human-readable error message (NULL for default)
     */
    void argparse_error_set(ArgParseErrorCategory category, int errno_val,
        const char* func, int line, const char* arg_name,
        const char* user_msg);

    /* @brief Clears the current thread's error state. */
    void argparse_error_clear(void);

    /**
     * @brief Returns the category of the last error that occurred in the current thread.
     * @return Error category of last error, or APE_SUCCESS if no error.
    */
    ArgParseErrorCategory argparse_error_get_category(void);

    /**
     * @brief Returns the errno value associated with the last error.
     * @return Standard C errno value, or 0 if no error.
    */
    int argparse_error_get_errno(void);

    /**
     * @brief Returns a formatted error message for the last error.
     * @return Formatted error message string, or empty string if no error.
    */
    const char* argparse_error_get_message(void);

    /**
     * @brief Returns the name of the argument that caused the last error.
     * @return Argument name, or empty string if no argument or no error.
    */
    const char* argparse_error_get_argument(void);

    /**
     * @brief Checks if an error has occurred in the current thread.
     * @return true if an error has occurred, false otherwise.
    */
    bool argparse_error_occurred(void);

    /**
     * @brief Determines if the last error is fatal (requires program termination).
     * @return true if error is fatal, false if non-fatal.
    */
    bool argparse_error_is_fatal(void);

    /**
     * @brief Converts an error category to a human-readable string.
     * @param category Error category to convert
     * @return String representation of error category.
    */
    const char* argparse_error_category_to_string(ArgParseErrorCategory category);

#define APE_SET(cat, err, arg, msg) \
    argparse_error_set((cat), (err), __func__, __LINE__, (arg), (msg))

#define APE_SET_MEMORY(arg) \
    APE_SET(APE_MEMORY, ENOMEM, (arg), "Memory allocation failed.")

#define APE_SET_SYNTAX(arg, msg) \
    APE_SET(APE_SYNTAX, EINVAL, (arg), (msg))

#define APE_SET_TYPE(arg, msg) \
    APE_SET(APE_TYPE, EINVAL, (arg), (msg))

#define APE_SET_REQUIRED(arg) \
    APE_SET(APE_REQUIRED, EINVAL, (arg), "Required argument not provided.")

#define APE_SET_RANGE(arg, msg) \
    APE_SET(APE_RANGE, ERANGE, (arg), (msg))

#define APE_SET_UNKNOWN(arg) \
    APE_SET(APE_UNKNOWN_ARG, EINVAL, (arg), "Unknown argument.")

#define APE_SET_DUPLICATE(arg) \
    APE_SET(APE_DUPLICATE, EEXIST, (arg), "Duplicate argument definition.")

#ifdef __cplusplus
}
#endif

#endif