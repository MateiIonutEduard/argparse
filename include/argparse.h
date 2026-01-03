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

    /* Create a new ArgParser instance, allocate memory, and set a short description. */
    ArgParser* argparse_new(const char* description);

    /* Free the memory of the given ArgParser* instance. */
    void argparse_free(ArgParser* parser);

    /* Define parsing for a single command-line argument. */
    void argparse_add_argument(ArgParser* parser, char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value);

    /* Define a command-line argument with configurable GNU-style suffix support. */
    void argparse_add_argument_ex(ArgParser* parser, char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value, char suffix);

    /* Define parsing for a typed command-line list argument. */
    void argparse_add_list_argument(ArgParser* parser, char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required);

    /* Define a command-line list argument with full GNU-style configuration. */
    void argparse_add_list_argument_ex(ArgParser* parser, char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required, char suffix, char delimiter);

    /* Parse the command-line arguments and store them in the ArgParser instance. */
    void argparse_parse(ArgParser* parser, int argc, char** argv);

    /* Display the help message and exit. */
    void argparse_print_help(ArgParser* parser);

    /* Return the boolean value of name from the parsed command-line arguments. */
    bool argparse_get_bool(ArgParser* parser, char* name);

    /* Return the integer value of the parameter for name. */
    int argparse_get_int(ArgParser* parser, char* name);

    /* Return the double value of the parameter for name. */
    double argparse_get_double(ArgParser* parser, char* name);

    /* Return the string value of the parameter for name. */
    const char* argparse_get_string(ArgParser* parser, char* name);

    /* Return the number of elements in the list for name. */
    int argparse_get_list_count(ArgParser* parser, char* name);

    /* Return a copy of the integer list for name. */
    int argparse_get_int_list(ArgParser* parser, char* name, int** values);

    /* Return a copy of the double list for name. */
    int argparse_get_double_list(ArgParser* parser, char* name, double** values);

    /* Return a copy of the string list for name. */
    int argparse_get_string_list(ArgParser* parser, char* name, char*** values);

    /* Free the memory of the integer list parameter. */
    void argparse_free_int_list(int** values, int count);

    /* Free the memory of the double list parameter. */
    void argparse_free_double_list(double** values, int count);

    /* Free the memory of the string list parameter. */
    void argparse_free_string_list(char*** values, int count);

    int argparse_get_last_error(void);

    const char* argparse_get_last_error_message(void);

    void argparse_clear_error(void);

#ifdef _MSC_VER
#define strdup argparse_strdup
#endif

#ifdef __cplusplus
}
#endif

#endif