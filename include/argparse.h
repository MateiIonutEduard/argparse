#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>

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

    /* Create a new ArgParser instance, allocate memory, and set a short description. */
    ArgParser* argparse_new(const char* description);

    /* Free the memory of the given ArgParser* instance. */
    void argparse_free(ArgParser* parser);

    /* argument management */
    void argparse_add_argument(ArgParser* parser, char short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value);
    void argparse_add_list_argument(ArgParser* parser, char short_name, const char* long_name,
        ArgType list_type, const char* help, bool required);

    /* Parse command-line arguments and store them in the ArgParser instance. */
    void argparse_parse(ArgParser* parser, int argc, char** argv);

    /* Display the help message and exit. */
    void argparse_print_help(ArgParser* parser);

    /* Return the boolean value of short_name from the parsed command-line arguments. */
    bool argparse_get_bool(ArgParser* parser, char short_name);

    /* Return the integer value of the parameter for short_name. */
    int argparse_get_int(ArgParser* parser, char short_name);

    /* Return the double value of the parameter for short_name. */
    double argparse_get_double(ArgParser* parser, char short_name);

    /* Return the string value of the parameter for short_name. */
    const char* argparse_get_string(ArgParser* parser, char short_name);

    /* list access functions */
    int argparse_get_list_count(ArgParser* parser, char short_name);
    int argparse_get_int_list(ArgParser* parser, char short_name, int** values);

    int argparse_get_double_list(ArgParser* parser, char short_name, double** values);
    int argparse_get_string_list(ArgParser* parser, char short_name, char*** values);

    /* utility functions */
    void argparse_free_int_list(int** values, int count);
    void argparse_free_double_list(double** values, int count);
    void argparse_free_string_list(char*** values, int count);
	
	#ifdef _MSC_VER
	#define strdup ap_strdup
	#endif

#ifdef __cplusplus
}
#endif

#endif