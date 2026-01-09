#include "argparse.h"
#include "argparse_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#define APE_RETURN_IF_ERROR(parser) \
    if (argparse_error_occurred()) { \
        if (argparse_error_is_fatal()) { \
            argparse_print_help(parser); \
            exit(EXIT_FAILURE); \
        } \
        return; \
    }

static bool safe_add_size_t(size_t a, size_t b, size_t* result) {
    /* would overflow */
    if (a > SIZE_MAX - b) {
        APE_SET(APE_RANGE, EOVERFLOW, NULL, "Size addition overflow.");
        return false;
    }

    *result = a + b;
    return true;
}

static bool safe_multiply_size_t(size_t a, size_t b, size_t* result) {
    /* would overflow */
    if (a > 0 && b > SIZE_MAX / a) {
        APE_SET(APE_RANGE, EOVERFLOW, NULL, "Size multiplication overflow.");
        return false;
    }
    
    *result = a * b;
    return true;
}

char* argparse_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    size_t total_size;

    if (!safe_add_size_t(len, 1, &total_size)) return NULL;
    char* copy = (char*)malloc(total_size);

    if (!copy) {
        APE_SET_MEMORY(NULL);
        return NULL;
    }

    memcpy(copy, str, total_size);
    return copy;
}

static void insert_argument_into_hash_table(ArgParser* parser, Argument* arg) {
    if (!parser || !arg) return;

    parser->argument_count++;

    /* insert into hash table if it exists */
    if (parser->hash_table) {
        if (arg->short_name)
            argparse_hash_insert_internal(parser->hash_table, arg->short_name, arg);

        if (arg->long_name)
            argparse_hash_insert_internal(parser->hash_table, arg->long_name, arg);
    }

    /* build hash table when threshold reached */
    else if (parser->argument_count >= ARGPARSE_HASH_THRESHOLD)
        ensure_hash_table_built(parser);
}

/* Internal list node structure */
typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

static bool is_list_type(ArgType type) {
    return ((type == ARG_INT_LIST)
        || (type == ARG_DOUBLE_LIST)
        || (type == ARG_STRING_LIST));
}

static void* create_default_value(ArgType type) {
    switch (type) {
    case ARG_INT: {
        int* val = (int*)malloc(sizeof(int));
        if (!val) return NULL;

        *val = 0;
        return val;
    }
    case ARG_DOUBLE: {
        double* val = (double*)malloc(sizeof(double));
        if (!val) return NULL;

        *val = 0.0;
        return val;
    }
    case ARG_STRING:
        return NULL;
    case ARG_BOOL: {
        bool* val = (bool*)malloc(sizeof(bool));
        if (!val) return NULL;

        *val = false;
        return val;
    }
    case ARG_INT_LIST:
    case ARG_DOUBLE_LIST:
    case ARG_STRING_LIST: {
        ListNode** list = (ListNode**)malloc(sizeof(ListNode*));
        if (!list) return NULL;

        *list = NULL;
        return list;
    }
    default:
        return NULL;
    }
}

static void append_to_list(Argument* arg, void* value) {
    /* validate inputs */
    if (!arg) {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "append_to_list: Argument pointer is NULL.");
        return;
    }

    if (!value) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "append_to_list: Cannot append NULL value to list.");
        return;
    }

    /* type safety verification */
    if (!arg->is_list) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "append_to_list: called on non-list argument.");
        return;
    }

    /* storage initialization check */
    if (!arg->value) {
        arg->value = malloc(sizeof(ListNode*));

        if (!arg->value) {
            const char* arg_name = arg->long_name ? arg->long_name :
                arg->short_name ? arg->short_name :
                "(unnamed)";
            APE_SET_MEMORY(arg_name);
            return;
        }

        /* initialize to empty list */
        *(ListNode**)arg->value = NULL;
    }

    /* type-safe pointer conversion */
    ListNode** head_ptr = (ListNode**)arg->value;

    /* double-check the cast result */
    if (!head_ptr) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "List head pointer is NULL after validation.");
        return;
    }

    /* node allocation */
    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));

    if (!new_node) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";

        /* clean up arg->value if we just allocated it */
        if (arg->value && *(ListNode**)arg->value == NULL) {
            free(arg->value);
            arg->value = NULL;
        }

        APE_SET_MEMORY(arg_name);
        return;
    }

    /* node initialization */
    new_node->data = value;
    new_node->next = NULL;

    /* list insertion */
    if (*head_ptr == NULL)
        *head_ptr = new_node;
    else {
        /* non-empty list - append to tail */
        ListNode* current = *head_ptr;

        /* O(n) but necessary for append */
        while (current->next != NULL)
            current = current->next;

        current->next = new_node;
    }
}

static int list_length(ListNode* head) {
    if (!head) return 0;
    int count = 0;

    while (head) {
        count++;
        head = head->next;
    }

    return count;
}

static bool get_safe_int(const char* str, int* out) {
    if (!str || !out) return false;

    /* skip leading whitespace manually */
    const char* p = str;

    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0') return false;

    char* endptr; errno = 0;
    long val = strtol(p, &endptr, 10);

    /* check if any conversion happened */
    if (endptr == p) return false;

    /* check for trailing non-whitespace chars */
    while (*endptr != '\0') {
        if (!isspace((unsigned char)*endptr))
            return false;
        endptr++;
    }

    /* check for overflow / underflow */
    if (errno == ERANGE) return false;

    /* check if value fits in int */
    if (val > INT_MAX || val < INT_MIN)
        return false;

    *out = (int)val;
    return true;
}

static bool get_safe_double(const char* str, double* out) {
    if (!str || !out) return false;
    const char* p = str;

    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0') return false;

    char* endptr; errno = 0;
    double val = strtod(p, &endptr);
    if (endptr == p) return false;

    /* check for trailing non-whitespace */
    while (*endptr != '\0') {
        if (!isspace((unsigned char)*endptr)) return false;
        endptr++;
    }

    /* check for math errors */
    if (errno == ERANGE || errno == EDOM)
        return false;

    if (fpclassify(val) == FP_INFINITE || fpclassify(val) == FP_NAN)
        return false;

    *out = val;
    return true;
}

/* Parse list values using dynamic delimiter using zero allocations for parsing. */
static void parse_list_with_delimiter(Argument* arg, const char* value_str) {
    /* clear any existing errors */
    argparse_error_clear();

    if (!arg || !value_str || !arg->is_list) {
        const char* arg_name = arg ? (arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)") : "(null)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name, "Invalid list argument.");
        return;
    }

    /* get dynamic delimiter per argument */
    const char delimiter = arg->delimiter ? arg->delimiter : ' ';
    const char* start = value_str;

    const char* end;
    int count = 0;

    while (*start) {
        /* skip leading delimiters */
        while (*start && *start == delimiter) start++;
        if (!*start) break;

        /* find token end */
        end = start;
        while (*end && *end != delimiter) end++;

        const size_t token_len = (size_t)(end - start);
        if (token_len == 0) break;

        /* parse based on type with minimal allocations */
        void* parsed_value = NULL;
        bool valid = false;

        switch (arg->type) {
        case ARG_INT_LIST: {
            /* stack buffer for safe parsing */
            char token_buf[32];

            if (token_len >= sizeof(token_buf)) {
                const char* arg_name = arg->long_name ? arg->long_name :
                    arg->short_name ? arg->short_name :
                    "(unnamed)";
                APE_SET(APE_RANGE, ERANGE, arg_name, "List value too long for integer parsing.");
                return;
            }

            memcpy(token_buf, start, token_len);
            token_buf[token_len] = '\0';
            int* val = (int*)malloc(sizeof(int));

            if (val && get_safe_int(token_buf, val)) {
                parsed_value = val;
                valid = true;
            }
            else if (val)
                free(val);
            break;
        }
        case ARG_DOUBLE_LIST: {
            char token_buf[64];
            if (token_len >= sizeof(token_buf)) {
                const char* arg_name = arg->long_name ? arg->long_name :
                    arg->short_name ? arg->short_name :
                    "(unnamed)";
                APE_SET(APE_RANGE, ERANGE, arg_name, "List value too long for double parsing.");
                return;
            }

            memcpy(token_buf, start, token_len);
            token_buf[token_len] = '\0';
            double* val = (double*)malloc(sizeof(double));

            if (val && get_safe_double(token_buf, val)) {
                parsed_value = val;
                valid = true;
            }
            else if (val)
                free(val);
            break;
        }
        case ARG_STRING_LIST: {
            /* strings require allocation anyway */
            char* val = (char*)malloc(token_len + 1);

            if (val) {
                memcpy(val, start, token_len);
                val[token_len] = '\0';
                parsed_value = val;
                valid = true;
            }

            break;
        }
        default: {
            const char* arg_name = arg->long_name ? arg->long_name :
                arg->short_name ? arg->short_name :
                "(unnamed)";
            APE_SET(APE_INTERNAL, EINVAL, arg_name, "Invalid list type.");

            return;
        }
        }

        if (valid) {
            append_to_list(arg, parsed_value);
            count++;
        }
        else {
            const char* arg_name = arg->long_name ? arg->long_name :
                arg->short_name ? arg->short_name :
                "(unnamed)";
            APE_SET(APE_TYPE, EINVAL, arg_name, "Invalid list value.");
            return;
        }

        start = end;
    }

    if (count == 0) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_SYNTAX, EINVAL, arg_name, "List requires values.");
        return;
    }

    arg->set = true;
}

/* Helper function to parse multiple values for list arguments. */
static int parse_list_values(ArgParser* parser, Argument* arg,
    int current_index, int argc, char** argv) {

    /* clear any existing errors */
    argparse_error_clear();

    /* validate inputs */
    if (!parser || !arg || !argv)
        return current_index;

    /* get the dynamic delimiter */
    const char delimiter = arg->delimiter
        ? arg->delimiter : ' ';

    int i = current_index + 1;
    int values_parsed = 0;

    while (i < argc && !argparse_hash_find_argument(parser, argv[i])) {
        const char* value = argv[i];

        /* check if value contains delimiter */
        if (delimiter != ' ') {
            const char* delim_pos = strchr(value, delimiter);

            if (delim_pos) {
                /* parse as delimited string */
                parse_list_with_delimiter(arg, value);

                /* check if parse_list_with_delimiter failed */
                if (argparse_error_occurred())
                    return current_index;

                values_parsed++;
                i++;
                continue;
            }
        }

        /* parse as single value */
        void* parsed_value = NULL;
        bool valid = false;

        switch (arg->type) {
        case ARG_INT_LIST: {
            int* val = (int*)malloc(sizeof(int));

            if (val && get_safe_int(value, val)) {
                parsed_value = val;
                valid = true;
            }
            else if (val)
                free(val);
            break;
        }
        case ARG_DOUBLE_LIST: {
            double* val = (double*)malloc(sizeof(double));

            if (val && get_safe_double(value, val)) {
                parsed_value = val;
                valid = true;
            }
            else if (val)
                free(val);
            break;
        }
        case ARG_STRING_LIST: {
            char* val = strdup(value);

            if (val) {
                parsed_value = val;
                valid = true;
            }
            break;
        }
        default:
            break;
        }

        if (valid) {
            append_to_list(arg, parsed_value);
            values_parsed++;
        }
        else {
            const char* arg_name = arg->long_name ? arg->long_name :
                arg->short_name ? arg->short_name :
                "(unnamed)";
            APE_SET(APE_TYPE, EINVAL, arg_name, "Invalid list value.");
            return current_index;
        }

        i++;
    }

    if (values_parsed == 0) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_SYNTAX, EINVAL, arg_name, "List argument requires values.");
        return current_index;
    }

    arg->set = true;
    return i - 1;
}

ArgParser* argparse_new(const char* description) {
    /* clear any existing errors */
    argparse_error_clear();
    ArgParser* parser = (ArgParser*)calloc(1, sizeof(ArgParser));

    if (!parser) {
        APE_SET_MEMORY(NULL);
        return NULL;
    }

    /* initialize all fields */
    parser->arguments = NULL;
    parser->program_name = NULL;
    parser->help_requested = false;
    parser->help_added = true;

    /* initialize hash table fields */
    parser->hash_table = NULL;
    parser->argument_count = 0;
    parser->hash_enabled = false;

    /* set the description */
    if (description) {
        parser->description = strdup(description);

        if (!parser->description) {
            APE_SET_MEMORY(NULL);
            free(parser);
            return NULL;
        }
    }
    else
        parser->description = NULL;

    /* automatically add help argument */
    argparse_add_argument(parser, "-h", "--help", ARG_BOOL,
        "Show this help message and exit", false, NULL);

    /* check if help argument addition failed */
    if (argparse_error_occurred()) {
        argparse_free(parser);
        return NULL;
    }

    return parser;
}

static void free_argument(Argument* arg) {
    if (!arg) return;

    /* free string fields */
    free(arg->short_name);

    free(arg->long_name);
    free(arg->help);

    /* free value based on type */
    if (arg->value != NULL) {
        switch (arg->type) {
        case ARG_INT:
        case ARG_DOUBLE:
        case ARG_BOOL:
            free(arg->value);
            break;

        case ARG_STRING:
            free(arg->value);
            break;

        case ARG_INT_LIST:
        case ARG_DOUBLE_LIST:
        case ARG_STRING_LIST: {
            ListNode* head = *(ListNode**)arg->value;

            /* free list nodes and their data */
            while (head != NULL) {
                ListNode* next = head->next;

                /* free node data based on type */
                free(head->data);

                /* free the node itself */
                free(head);
                head = next;
            }

            free(arg->value);
            break;
        }
        }
    }

    /* finally free the argument struct */
    free(arg);
}

/* High-performance help argument detection function without prefix dependency. */
static bool is_help_argument(const char* arg_name) {
    /* quick rejection of NULL or empty strings */
    if (!arg_name || arg_name[0] == '\0')
        return false;

    /* block all format string attacks */
    if (strchr(arg_name, '%'))
        return false;

    /* pre-computed list of accepted help arguments */
    static const char* const patterns[] = {
        "-h", "-H", "--help", "--HELP",
        "/?", "/help", "/HELP", NULL
    };

    /* linear scan through accepted forms, O(1) in practice */
    for (const char* const* ptr = patterns; *ptr; ptr++) {
        if (strcmp(arg_name, *ptr) == 0)
            return true;
    }

    return false;
}

void argparse_add_argument(ArgParser* parser, const char* short_name, const char* long_name,
    ArgType type, const char* help, bool required, void* default_value) {
    argparse_error_clear();

    /* validate parser parameter */
    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return;
    }

    /* don't add duplicate help arguments */
    if (!parser->help_added && ((short_name && is_help_argument(short_name)) ||
        (long_name && is_help_argument(long_name))))
        return;

    /* validate argument names */
    if ((!short_name || short_name[0] == '\0') &&
        (!long_name || long_name[0] == '\0')) {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "Both short and long names are empty.");
        return;
    }

    /* get argument name for error reporting */
    const char* arg_name = short_name ? short_name :
        long_name ? long_name : "(unnamed)";

    /* alloc argument structure */
    Argument* arg = (Argument*)malloc(sizeof(Argument));
    if (!arg) {
        APE_SET_MEMORY(arg_name);
        return;
    }

    /* init all fields to known state */
    memset(arg, 0, sizeof(Argument));
    arg->required = required;
    arg->type = type;
    arg->is_list = is_list_type(type);
    arg->delimiter = ' ';

    /* duplicate strings with immediate error checking */
    if (short_name && !(arg->short_name = strdup(short_name))) goto memory_error;
    if (long_name && !(arg->long_name = strdup(long_name))) goto memory_error;
    if (help && !(arg->help = strdup(help))) goto memory_error;

    /* handle default value allocation */
    if (default_value) {
        switch (type) {
        case ARG_INT:
            if (!(arg->value = malloc(sizeof(int)))) goto memory_error;
            *(int*)arg->value = *(int*)default_value;
            break;

        case ARG_DOUBLE:
            if (!(arg->value = malloc(sizeof(double)))) goto memory_error;
            *(double*)arg->value = *(double*)default_value;
            break;

        case ARG_STRING:
            arg->value = strdup((char*)default_value);
            if (!arg->value && default_value) goto memory_error;
            break;

        case ARG_BOOL:
            if (!(arg->value = malloc(sizeof(bool)))) goto memory_error;
            *(bool*)arg->value = *(bool*)default_value;
            break;

        default:
            arg->value = create_default_value(type);
            if (!arg->value && type != ARG_STRING) goto memory_error;
            break;
        }
    }
    else {
        arg->value = create_default_value(type);
        if (!arg->value && type != ARG_STRING) goto memory_error;
    }

    /* add to linked list - maintain tail for O(1) append */
    if (!parser->arguments) {
        parser->arguments = arg;
    }
    else {
        Argument* tail = parser->arguments;
        while (tail->next) tail = tail->next;
        tail->next = arg;
    }

    /* handle hash table integration */
    insert_argument_into_hash_table(parser, arg);
    return;

memory_error:
    APE_SET_MEMORY(arg_name);
    free_argument(arg);
}

void argparse_add_argument_ex(ArgParser* parser, const char* short_name, const char* long_name,
    ArgType type, const char* help, bool required, void* default_value, char suffix) {
    argparse_error_clear();

    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return;
    }

    /* use existing memory allocation */
    argparse_add_argument(parser, short_name, long_name, type, help, required, default_value);
    if (argparse_error_occurred()) return;

    /* find the last added argument efficiently */
    Argument* arg = parser->arguments;
    Argument* last = NULL;

    while (arg) {
        last = arg;
        arg = arg->next;
    }

    if (last)
        last->suffix = suffix;
}

/* Skip dynamic prefixes for a specific argument.  */
static inline const char* skip_dynamic_prefix(const char* str) {
    if (!str) return NULL;

    while (*str && !isalnum((unsigned char)*str))
        str++;

    return str;
}

/* Calculate clean name length after skipping prefix. */
static inline size_t clean_name_length(const char* name) {
    const char* clean = skip_dynamic_prefix(name);
    if (!clean) return 0;
    return strlen(clean);
}

/* Single-pass GNU-style argument detector with dynamic suffix. */
static Argument* is_gnu_argument(ArgParser* parser, const char* arg_str, const char** value_ptr) {
    if (!parser || !arg_str || !value_ptr) return NULL;
    Argument* current = parser->arguments;

    while (current) {
        /* skip arguments without GNU-style enabled */
        if (current->suffix == 0) {
            current = current->next;
            continue;
        }

        /* look for this argument's specific suffix char */
        const char* suffix_pos = strchr(arg_str, current->suffix);
        if (!suffix_pos) {
            current = current->next;
            continue;
        }

        /* calculate total length before suffix */
        size_t total_len = (size_t)(suffix_pos - arg_str);

        if (total_len == 0) {
            current = current->next;
            continue;
        }

        /* skip prefix to compare clean names */
        const char* arg_clean = skip_dynamic_prefix(arg_str);

        if (!arg_clean || arg_clean >= suffix_pos) {
            current = current->next;
            continue;
        }

        /* calculate clean name length */
        size_t clean_len = (size_t)(suffix_pos - arg_clean);

        /* check against short name */
        if (current->short_name) {
            const char* short_clean = skip_dynamic_prefix(current->short_name);

            if (short_clean && strlen(short_clean) == clean_len) {
                if (strncmp(arg_clean, short_clean, clean_len) == 0) {
                    *value_ptr = suffix_pos + 1;
                    return current;
                }
            }
        }

        /* check against long name */
        if (current->long_name) {
            const char* long_clean = skip_dynamic_prefix(current->long_name);
            if (long_clean && strlen(long_clean) == clean_len) {
                if (strncmp(arg_clean, long_clean, clean_len) == 0) {
                    *value_ptr = suffix_pos + 1;
                    return current;
                }
            }
        }

        current = current->next;
    }

    return NULL;
}

void argparse_add_list_argument(ArgParser* parser, const char* short_name, const char* long_name,
    ArgType list_type, const char* help, bool required) {
    argparse_error_clear();

    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return;
    }

    argparse_add_argument(parser, short_name, long_name, list_type, help, required, NULL);
}

void argparse_add_list_argument_ex(ArgParser* parser, const char* short_name, const char* long_name,
    ArgType list_type, const char* help, bool required, char suffix, char delimiter) {
    argparse_error_clear();

    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return;
    }

    if (!is_list_type(list_type)) {
        const char* arg_name = short_name ? short_name :
            long_name ? long_name : "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name, "Invalid list type.");
        return;
    }

    argparse_add_argument_ex(parser, short_name, long_name, list_type,
        help, required, NULL, suffix);

    /* an existing error is thrown, stop the execution */
    if (argparse_error_occurred())
        return;

    /* find and set the delimiter */
    Argument* arg = parser->arguments;
    Argument* last = NULL;

    while (arg) {
        last = arg;
        arg = arg->next;
    }

    if (last)
        last->delimiter = delimiter;
    else {
        const char* arg_name = short_name ? short_name :
            long_name ? long_name : "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name, "No arguments in parser.");
    }
}

static void parse_single_value(Argument* arg, const char* str_val) {
    argparse_error_clear();

    /* validate the inputs */
    if (!arg || !str_val) {
        const char* arg_name = arg ? (arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)") : "(null)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "Invalid argument or value pointer.");
        return;
    }

    /* validate that it is not processing a list argument */
    if (is_list_type(arg->type)) {
        const char* arg_name = arg->long_name ? arg->long_name :
            arg->short_name ? arg->short_name :
            "(unnamed)";
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "List argument processed in parse_single_value.");
        return;
    }

    /* get argument name for error reporting */
    const char* arg_name = arg->long_name ? arg->long_name :
        arg->short_name ? arg->short_name :
        "(unnamed)";

    /* handle each argument type with proper error checking */
    switch (arg->type) {
    case ARG_INT:
        if (arg->value) {
            int parsed_val = 0;

            if (!get_safe_int(str_val, &parsed_val)) {
                APE_SET(APE_TYPE, EINVAL, arg_name,
                    "Invalid integer value.");
                return;
            }

            *(int*)arg->value = parsed_val;
        }
        else {
            APE_SET(APE_INTERNAL, EINVAL, arg_name,
                "Argument value pointer is NULL.");
            return;
        }
        break;

    case ARG_DOUBLE:
        if (arg->value != NULL) {
            double parsed_val = 0.0;

            if (!get_safe_double(str_val, &parsed_val)) {
                APE_SET(APE_TYPE, EINVAL, arg_name,
                    "Invalid floating-point value.");
                return;
            }

            *(double*)arg->value = parsed_val;
        }
        else {
            APE_SET(APE_INTERNAL, EINVAL, arg_name,
                "Argument value pointer is NULL.");
            return;
        }
        break;

    case ARG_STRING: {
        char* new_value = strdup(str_val);
        if (!new_value) {
            APE_SET_MEMORY(arg_name);
            return;
        }

        /* Free previous value if it exists */
        if (arg->value)
            free(arg->value);

        arg->value = new_value;
        break;
    }

    case ARG_BOOL:
        if (arg->value) {
            /* handle flag-style boolean */
            if (str_val[0] == '\0')
                *(bool*)arg->value = true;
            else {
                /* convert to lowercase */
                char lower_val[64];
                size_t i = 0;

                for (; str_val[i] && i < sizeof(lower_val) - 1; i++)
                    lower_val[i] = (char)tolower((unsigned char)str_val[i]);
                
                lower_val[i] = '\0';
                printf("-verbose: %s\n", lower_val);

                /* check for overflow */
                if (str_val[i] != '\0') {
                    APE_SET(APE_RANGE, ERANGE, arg_name,
                        "Boolean value too long.");
                    return;
                }

                /* explicit true values (case-insensitive) */
                if (strcmp(lower_val, "true") == 0 ||
                    strcmp(lower_val, "1") == 0 ||
                    strcmp(lower_val, "yes") == 0 ||
                    strcmp(lower_val, "on") == 0 ||
                    strcmp(lower_val, "enable") == 0 ||
                    strcmp(lower_val, "enabled") == 0) {
                    *(bool*)arg->value = true;
                }
                /* explicit false values (case-insensitive) */
                else if (strcmp(lower_val, "false") == 0 ||
                    strcmp(lower_val, "0") == 0 ||
                    strcmp(lower_val, "no") == 0 ||
                    strcmp(lower_val, "off") == 0 ||
                    strcmp(lower_val, "disable") == 0 ||
                    strcmp(lower_val, "disabled") == 0) {
                    *(bool*)arg->value = false;
                }
                else {
                    APE_SET(APE_TYPE, EINVAL, arg_name,
                        "Invalid boolean value. Use: true/false, "
                        "yes/no, 1/0, on/off, enable/disable");
                    return;
                }
            }
        }
        else {
            APE_SET(APE_INTERNAL, EINVAL, arg_name,
                "Argument value pointer is NULL.");
            return;
        }

        break;

    default:
        /* unknown argument type */
        APE_SET(APE_INTERNAL, EINVAL, arg_name,
            "Unknown argument type.");
        return;
    }

    arg->set = true;
}

void argparse_parse(ArgParser* parser, int argc, char** argv) {
    argparse_error_clear();

    /* check for valid inputs */
    if (!parser || !argv) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Invalid parser or argv.");
        APE_RETURN_IF_ERROR(parser);
        return;
    }

    /* store program name */
    if (parser->program_name != NULL)
        free(parser->program_name);

    parser->program_name = strdup(argv[0]);
    if (!parser->program_name) {
        APE_SET_MEMORY("program_name");
        APE_RETURN_IF_ERROR(parser);
        return;
    }

    /* check if no arguments were provided */
    if (argc == 1) {
        argparse_print_help(parser);
        APE_SET(APE_HELP_REQUESTED, 0, NULL, "No arguments provided, showing help.");
        APE_RETURN_IF_ERROR(parser);
        return;
    }

    /* process command line arguments */
    for (int i = 1; i < argc; i++) {
        const char* current_arg = argv[i];

        /* GNU-style argument detection */
        const char* gnu_value = NULL;
        Argument* gnu_arg = is_gnu_argument(parser, current_arg, &gnu_value);

        if (gnu_arg) {
            /* process GNU-style argument */
            if (gnu_arg->type == ARG_BOOL) {
                parse_single_value(gnu_arg, gnu_value[0] ? gnu_value : "true");
                APE_RETURN_IF_ERROR(parser);
            }
            else if (gnu_arg->is_list) {
                parse_list_with_delimiter(gnu_arg, gnu_value);
                APE_RETURN_IF_ERROR(parser);
            }
            else {
                parse_single_value(gnu_arg, gnu_value);
                APE_RETURN_IF_ERROR(parser);
            }
            continue;
        }

        /* handle special help argument */
        if (is_help_argument(current_arg)) {
            parser->help_requested = true;
            argparse_print_help(parser);
            APE_SET(APE_HELP_REQUESTED, 0, NULL, "Help requested by user.");
            APE_RETURN_IF_ERROR(parser);
            return;
        }

        /* single efficient lookup */
        Argument* arg = argparse_hash_find_argument(parser, current_arg);

        if (arg) {
            /* Found argument, process based on type */
            if (arg->type == ARG_BOOL) {
                parse_single_value(arg, "");
                APE_RETURN_IF_ERROR(parser);
            }
            else if (arg->is_list) {
                i = parse_list_values(parser, arg, i, argc, argv);
                APE_RETURN_IF_ERROR(parser);
            }
            else if (i + 1 < argc) {
                /* check if next token is not a registered argument */
                if (!argparse_hash_is_argument(parser, argv[i + 1])) {
                    parse_single_value(arg, argv[++i]);
                    APE_RETURN_IF_ERROR(parser);
                }
                else {
                    APE_SET(APE_SYNTAX, EINVAL,
                        arg->short_name ? arg->short_name : arg->long_name,
                        "Option requires a value.");
                    APE_RETURN_IF_ERROR(parser);
                    return;
                }
            }
            else {
                APE_SET(APE_SYNTAX, EINVAL,
                    arg->short_name ? arg->short_name : arg->long_name,
                    "Option requires a value but none provided.");
                APE_RETURN_IF_ERROR(parser);
                return;
            }
        }
        else {
            /* not a registered argument */
            APE_SET(APE_SYNTAX, EINVAL, current_arg,
                "Unexpected value (did you forget an option?).");
            APE_RETURN_IF_ERROR(parser);
            return;
        }
    }

    /* validate required arguments */
    Argument* current = parser->arguments;
    while (current != NULL) {
        if (current->required && !current->set) {
            const char* arg_name = current->short_name ? current->short_name :
                current->long_name ? current->long_name :
                "(unnamed)";

            APE_SET(APE_REQUIRED, EINVAL, arg_name,
                "Required argument not provided.");
            APE_RETURN_IF_ERROR(parser);
            return;
        }
        current = current->next;
    }
}

bool argparse_get_bool(ArgParser* parser, const char* name) {
    if (parser == NULL) return false;
    Argument* arg = argparse_hash_find_argument(parser, name);
    return arg && arg->set ? *(bool*)arg->value : false;
}

int argparse_get_int(ArgParser* parser, const char* name) {
    if (parser == NULL) return 0;
    Argument* arg = argparse_hash_find_argument(parser, name);
    return arg && arg->set ? *(int*)arg->value : 0;
}

double argparse_get_double(ArgParser* parser, const char* name) {
    if (parser == NULL) return 0.0;
    Argument* arg = argparse_hash_find_argument(parser, name);
    return arg && arg->set ? *(double*)arg->value : 0.0;
}

const char* argparse_get_string(ArgParser* parser, const char* name) {
    if (parser == NULL) return NULL;
    Argument* arg = argparse_hash_find_argument(parser, name);
    return arg && arg->set ? (const char*)arg->value : NULL;
}

int argparse_get_list_count(ArgParser* parser, const char* name) {
    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return 0;
    }

    Argument* arg = argparse_hash_find_argument(parser, name);
    if (!arg || !arg->set) return 0;

    ListNode* head = *(ListNode**)arg->value;
    return list_length(head);
}

int argparse_get_int_list(ArgParser* parser, const char* name, int** values) {
    /* clear any existing errors */
    argparse_error_clear();

    /* check inputs first */
    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return 0;
    }

    if (!name || name[0] == '\0') {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "Argument name are empty or NULL.");
        return 0;
    }

    if (!values) {
        APE_SET_MEMORY(values);
        return 0;
    }

    /* find the argument */
    Argument* arg = argparse_hash_find_argument(parser, name);

    if (!arg || !arg->set || arg->type != ARG_INT_LIST)
        return 0;

    /* get list head */
    ListNode* head = *(ListNode**)arg->value;

    if (!head) {
        *values = NULL;
        return 0;
    }

    /* count elements and allocate memory */
    int count = list_length(head);

    if (count == 0) {
        *values = NULL;
        return 0;
    }

    size_t alloc_size;

    if (!safe_multiply_size_t((size_t)count, sizeof(int), &alloc_size)) {
        APE_SET(APE_RANGE, EOVERFLOW, name, "List size overflow.");
        *values = NULL;
        return 0;
    }

    /* alloc memory for the array with overflow protection */
    int* array = (int*)malloc((size_t)count * sizeof(int));

    if (!array) {
        APE_SET_MEMORY(name);
        *values = NULL;
        return 0;
    }

    /* copy data from linked list to contiguous array */
    ListNode* current = head;
    int i = 0;

    while (current != NULL && i < count) {
        if (current->data != NULL)
            array[i] = *(int*)current->data;
        else array[i] = 0;

        current = current->next;
        i++;
    }

    *values = array;
    return count;
}

int argparse_get_double_list(ArgParser* parser, const char* name, double** values) {
    /* clear any existing errors */
    argparse_error_clear();

    /* check inputs first */
    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return 0;
    }

    if (!name || name[0] == '\0') {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "Argument name are empty or NULL.");
        return 0;
    }

    if (!values) {
        APE_SET_MEMORY(values);
        return 0;
    }

    /* find the argument */
    Argument* arg = argparse_hash_find_argument(parser, name);

    if (!arg || !arg->set || arg->type != ARG_DOUBLE_LIST)
        return 0;

    /* get list head and count */
    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);

    if (count == 0) {
        *values = NULL;
        return 0;
    }

    size_t alloc_size;

    if (!safe_multiply_size_t((size_t)count, sizeof(double), &alloc_size)) {
        APE_SET(APE_RANGE, EOVERFLOW, name, "List size overflow.");
        *values = NULL;
        return 0;
    }

    /* alloc memory for the array */
    double* array = (double*)malloc((size_t)count * sizeof(double));

    if (array == NULL) {
        APE_SET_MEMORY(name);
        *values = NULL;
        return 0;
    }

    /* copy data from linked list to array */
    ListNode* current = head;

    for (int i = 0; i < count && current; i++) {
        if (current->data)
            array[i] = *(double*)current->data;
        else array[i] = 0.0;
        current = current->next;
    }

    *values = array;
    return count;
}

int argparse_get_string_list(ArgParser* parser, const char* name, char*** values) {
    /* clear any existing errors */
    argparse_error_clear();

    /* check inputs first */
    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return 0;
    }

    if (!name || name[0] == '\0') {
        APE_SET(APE_INTERNAL, EINVAL, NULL,
            "Argument name are empty or NULL.");
        return 0;
    }

    if (!values) {
        APE_SET_MEMORY(values);
        return 0;
    }

    /* find the argument */
    Argument* arg = argparse_hash_find_argument(parser, name);

    if (!arg || !arg->set || arg->type != ARG_STRING_LIST)
        return 0;

    /* get list head and count */
    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);

    if (count == 0) {
        *values = NULL;
        return 0;
    }

    size_t alloc_size;

    if (!safe_multiply_size_t((size_t)count, sizeof(char*), &alloc_size)) {
        APE_SET(APE_RANGE, EOVERFLOW, name, "List size overflow.");
        *values = NULL;
        return 0;
    }

    /* alloc array for string pointers */
    char** string_array = (char**)malloc((size_t)count * sizeof(char*));

    if (!string_array) {
        APE_SET_MEMORY(name);
        *values = NULL;
        return 0;
    }

    /* duplicate all strings from the list */
    ListNode* current = head;
    int i = 0;

    while (current && i < count) {
        char* source_str = (char*)current->data;

        if (source_str) {
            string_array[i] = strdup(source_str);

            if (!string_array[i]) {
                /* clean up on failure and return */
                APE_SET_MEMORY(name);

                for (int j = 0; j < i; j++)
                    free(string_array[j]);

                free(string_array);
                *values = NULL;
                return 0;
            }
        }
        else
            /* handle NULL strings in the list */
            string_array[i] = NULL;

        current = current->next;
        i++;
    }

    *values = string_array;
    return count;
}

void argparse_free_int_list(int** values) {
    if (!values || !*values) return;
    free(*values);
    *values = NULL;
}

void argparse_free_double_list(double** values) {
    if (!values || !*values) return;
    free(*values);
    *values = NULL;
}

void argparse_free_string_list(char*** values, int count) {
    if (!values || !*values) return;

    for (int i = 0; i < count; i++)
        free((*values)[i]);

    free(*values);
    *values = NULL;
}

void argparse_print_help(ArgParser* parser) {
    /* clean up error system */
    argparse_error_clear();

    if (!parser) {
        APE_SET(APE_INTERNAL, EINVAL, NULL, "Parser is NULL.");
        return;
    }

    /* print usage header */
    fputs("Usage: ", stdout);
    fputs(parser->program_name ? parser->program_name : "", stdout);
    fputs(" [OPTIONS]\n\n", stdout);

    /* print description if available */
    if (parser->description && parser->description[0] != '\0') {
        fputs(parser->description, stdout);
        fputs("\n\n", stdout);
    }

    /* iterate through arguments */
    Argument* arg = parser->arguments;

    while (arg) {
        /* argument names */
        printf("  ");

        if (arg->short_name)
            printf("%s", arg->short_name);

        if (arg->long_name) {
            if (arg->short_name)
                printf(", ");
            
            printf("%s", arg->long_name);
        }

        /* Argument value placeholder */
        switch (arg->type) {
        case ARG_INT:
        case ARG_DOUBLE:
        case ARG_STRING:
            printf(" VALUE");
            break;
        case ARG_INT_LIST:
        case ARG_DOUBLE_LIST:
        case ARG_STRING_LIST:
            printf(" VALUE1 VALUE2 ...");
            break;
        default:
            break;
        }

        /* help text and required flag */
        printf("\n    %s", arg->help != NULL ? arg->help : "");

        if (arg->required)
            printf(" [required]");

        printf("\n");
        arg = arg->next;
    }
}

int argparse_get_last_error(void) {
    return argparse_error_get_errno();
}

const char* argparse_get_last_error_message(void) {
    return argparse_error_get_message();
}

void argparse_clear_error(void) {
    argparse_error_clear();
}

void argparse_free(ArgParser* parser) {
    if (!parser) return;

    /* clean up error system for this thread */
    argparse_error_clear();

    if (parser->hash_table) {
        argparse_hash_destroy_internal(parser->hash_table);
        parser->hash_table = NULL;
    }

    Argument* current = parser->arguments;

    while (current) {
        Argument* next = current->next;
        free_argument(current);
        current = next;
    }

    free(parser->program_name);
    free(parser->description);
    free(parser);
}