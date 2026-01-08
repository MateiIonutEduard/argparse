#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>
#include "argparse_error.h"
#include "argparse_hash.h"
#ifdef __cplusplus
extern "C" {
#endif

    typedef enum ArgType ArgType;
    typedef struct Argument Argument;
    typedef struct ArgParser ArgParser;

    enum ArgType {
        ARG_INT,
        ARG_DOUBLE,
        ARG_STRING,
        ARG_BOOL,
        ARG_INT_LIST,
        ARG_DOUBLE_LIST,
        ARG_STRING_LIST
    };

    struct Argument {
        char* short_name;
        char* long_name;

        char* help;
        ArgType type;

        void* value;
        bool required;
        bool set;

        unsigned char suffix;
        unsigned char delimiter;

        bool is_list;
        Argument* next;
    };

    struct ArgParser {
        Argument* arguments;
        char* program_name;

        char* description;
        bool help_requested;
        bool help_added;

        ArgHashTable* hash_table;
        size_t argument_count;
        bool hash_enabled;
    };

    /**
     * @brief Creates and initializes a new argument parser instance. 
     * @param description Brief program description for help output (can be NULL)
     * @return Pointer to new parser instance on success or NULL, if memory allocation fails.
     */
    ArgParser* argparse_new(const char* description);

    /**
     * @brief Releases all memory associated with a parser instance. 
     * @param parser Parser to free (NULL-safe)
     */
    void argparse_free(ArgParser* parser);

    /**
     * @brief Defines a command-line argument with basic configuration.
     * @param parser Target parser instance
     * @param short_name Single-character option (e.g., "-v", can be NULL)
     * @param long_name Multi-character option (e.g., "--verbose", can be NULL)
     * @param type Data type for argument value
     * @param help Description for help output (can be NULL)
     * @param required Whether argument must be provided
     * @param default_value Default value if not provided (type-specific, can be NULL)
     */
    void argparse_add_argument(ArgParser* parser, const char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value);

    /**
     * @brief Defines argument with GNU-style suffix support (e.g., -ofile or --output=file).
     * @param All parameters from argparse_add_argument function
     * @param suffix Delimiter character between argument name and value (e.g., '=')
     */
    void argparse_add_argument_ex(ArgParser* parser, const char* short_name, const char* long_name,
        ArgType type, const char* help, bool required, void* default_value, char suffix);

    /**
     * @brief Defines a list-type argument with default delimiter (space).
     * @param All Same parameters as into the base function
     * @param list_type Must be one of these types: ARG_INT_LIST, ARG_DOUBLE_LIST, ARG_STRING_LIST
     */
    void argparse_add_list_argument(ArgParser* parser, const char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required);

    /** 
     * @brief Defines list argument with full configuration options.
     * @param All parameters from argparse_add_list_argument
     * @param suffix GNU-style suffix character (see argparse_add_argument_ex)
     * @param delimiter Character separating values within a single argument
     */
    void argparse_add_list_argument_ex(ArgParser* parser, const char* short_name, const char* long_name,
        ArgType list_type, const char* help, bool required, char suffix, char delimiter);

    /**
     * @brief Parses command-line arguments according to defined specifications.
     * @param parser Configured parser instance
     * @param argc Argument count from main()
     * @param argv Argument vector from main()
     */
    void argparse_parse(ArgParser* parser, int argc, char** argv);

    /**
     * @brief Prints formatted usage information to stdout.
     * @param parser Parser instance (NULL-safe)
     */
    void argparse_print_help(ArgParser* parser);

    /**
     * @brief Retrieves boolean argument value.
     * @param parser Parser instance
     * @param Argument name (short or long form)
     * @return Boolean value (false if not set or error).
     */
    bool argparse_get_bool(ArgParser* parser, const char* name);

    /**
     * @brief Retrieves integer argument value.
     * @param parser Parser instance
     * @param Argument name (short or long form)
     * @return Integer value (0 if not set or error).
     */
    int argparse_get_int(ArgParser* parser, const char* name);

    /**
     * @brief Retrieves double-precision floating-point value.
     * @param parser Parser instance
     * @param Argument name (short or long form)
     * @return Double value (0.0 if not set or error).
     */
    double argparse_get_double(ArgParser* parser, const char* name);

    /**
     * @brief Retrieves string argument value.
     * @param parser Parser instance
     * @param Argument name (short or long form)
     * @return String pointer (NULL if not set or error).
     */
    const char* argparse_get_string(ArgParser* parser, const char* name);

    /**
     * @brief Returns number of elements in a list argument.
     * @param parser Parser instance
     * @param name List argument name
     * @return Element count (0 if empty, not found, or error).
     */
    int argparse_get_list_count(ArgParser* parser, const char* name);

    /**
     * @brief Retrieves integer list as dynamically allocated array.
     * @param parser Parser instance
     * @param name List argument name
     * @param values Pointer to receive allocated array
     * @return Number of elements retrieved (0 on error).
     */
    int argparse_get_int_list(ArgParser* parser, const char* name, int** values);

    /**
     * @brief Retrieves double list as dynamically allocated array.
     * @param parser Parser instance
     * @param name List argument name
     * @param values Pointer to receive allocated array
     * @return Number of elements retrieved (0 on error).
     */
    int argparse_get_double_list(ArgParser* parser, const char* name, double** values);

    /**
     * @brief Retrieves string list as dynamically allocated array of strings.
     * @param parser Parser instance
     * @param name List argument name
     * @param values Pointer to receive allocated array
     * @return Number of elements retrieved (0 on error).
     */
    int argparse_get_string_list(ArgParser* parser, const char* name, char*** values);

    /** 
     * @brief Frees memory allocated by argparse_get_int_list().
     * @param values Pointer to array pointer (set to NULL after free)
     */
    void argparse_free_int_list(int** values);

    /**
     * @brief Frees memory allocated by argparse_get_double_list().
     * @param values Pointer to array pointer (set to NULL after free)
     */
    void argparse_free_double_list(double** values);

    /**
     * @brief Frees memory allocated by argparse_get_string_list().
     * @param values Pointer to array-of-strings pointer
     * @param count Number of strings to free
     */
    void argparse_free_string_list(char*** values, int count);

    /**
     * @brief Retrieves last error code from thread-local error state.
     * @return Standard errno value (0 for success).
    */
    int argparse_get_last_error(void);

    /** @brief Retrieves formatted error message with context.
     * @return Human-readable error string (empty if no error).
    */
    const char* argparse_get_last_error_message(void);

    /**
     * @brief Resets error state for current thread.
    */
    void argparse_clear_error(void);

#ifdef _MSC_VER
#define strdup argparse_strdup
#endif

#ifdef __cplusplus
}
#endif

#endif