#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>
#include "argparse_error.h"
#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        ARG_INT,
        ARG_DOUBLE,
        ARG_STRING,
        ARG_BOOL,
        ARG_INT_LIST,
        ARG_DOUBLE_LIST,
        ARG_STRING_LIST
    } ArgType;

    typedef struct Argument Argument;
    typedef struct ArgParser ArgParser;
    typedef enum ArgType ArgType;

    /* @brief Create a new ArgParser instance, allocate memory, and set a short description. */
    ArgParser* argparse_new(const char* description);

    /* @brief Free the memory of the given ArgParser* instance. */
    void argparse_free(ArgParser* parser);

    /* @brief Define parsing for a single command-line argument. */
    void argparse_add_argument(ArgParser* parser, char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value);

    /* @brief Define a command-line argument with configurable GNU-style suffix support. */
    void argparse_add_argument_ex(ArgParser* parser, char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value, char suffix);

    /* @brief Define parsing for a typed command-line list argument. */
    void argparse_add_list_argument(ArgParser* parser, char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required);

    /* @brief Define a command-line list argument with full GNU-style configuration. */
    void argparse_add_list_argument_ex(ArgParser* parser, char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required, char suffix, char delimiter);

    /* @brief Parse the command-line arguments and store them in the ArgParser instance. */
    void argparse_parse(ArgParser* parser, int argc, char** argv);

    /* @brief Display the help message and exit. */
    void argparse_print_help(ArgParser* parser);

    /* @brief Return the boolean value of name from the parsed command-line arguments. */
    bool argparse_get_bool(ArgParser* parser, char* name);

    /* @brief Return the integer value of the parameter for name. */
    int argparse_get_int(ArgParser* parser, char* name);

    /* @brief Return the double value of the parameter for name. */
    double argparse_get_double(ArgParser* parser, char* name);

    /* @brief Return the string value of the parameter for name. */
    const char* argparse_get_string(ArgParser* parser, char* name);

    /* @brief Return the number of elements in the list for name. */
    int argparse_get_list_count(ArgParser* parser, char* name);

    /* @brief Return a copy of the integer list for name. */
    int argparse_get_int_list(ArgParser* parser, char* name, int** values);

    /* @brief Return a copy of the double list for name. */
    int argparse_get_double_list(ArgParser* parser, char* name, double** values);

    /* @brief Return a copy of the string list for name. */
    int argparse_get_string_list(ArgParser* parser, char* name, char*** values);

    /* @brief Free the memory of the integer list parameter. */
    void argparse_free_int_list(int** values, int count);

    /* @brief Free the memory of the double list parameter. */
    void argparse_free_double_list(double** values, int count);

    /* @brief Free the memory of the string list parameter. */
    void argparse_free_string_list(char*** values, int count);

    /**
     * @brief Get the last error code from the argparse error system.
     * 
     * Returns the `errno` value associated with the last error that occurred
     * in the current thread's argparse operations. 
     * 
     * This is a convenience wrapper around `argparse_error_get_errno()`.
     * 
     * @return The last error code (0 for success, standard errno values for errors).
    */
    int argparse_get_last_error(void);

    /** @brief Get a human-readable error message for the last argparse error.
     *
     * Returns a formatted error message describing the last error that occurred
     * in the current thread's argparse operations. The message includes error
     * category and context information. This is a convenience wrapper around
     * `argparse_error_get_message()`.
     * 
     * @return Formatted error message string, or empty string if no error.
    */
    const char* argparse_get_last_error_message(void);

    /**
     * @brief Clear the error state for the current thread.
     * Resets the argparse error system state for the current thread, clearing
     * any previously recorded errors. This is a convenience wrapper around
     * `argparse_error_clear()`.
    */
    void argparse_clear_error(void);

#ifdef _MSC_VER
#define strdup argparse_strdup
#endif

#ifdef __cplusplus
}
#endif

#endif