#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

/* Return a null-terminated duplicate of the string referenced by str. */
static char* ap_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;

    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* Internal list node structure */
typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

struct Argument {
    char* short_name;
    char* long_name;

    char* help;
    ArgType type;

    void* value;
    bool required;
    bool set;

    /* track if this is a list argument */
    bool is_list;
    Argument* next;
};

struct ArgParser {
    Argument* arguments;
    char* program_name;

    char* description;
    bool help_requested;
};

static Argument* find_argument(ArgParser* parser, char* name) {
    /* early return for invalid cases */
    if (!parser || !name || name[0] == '\0')
        return NULL;

    Argument* arg = parser->arguments;

    while (arg != NULL) {
        if (arg->short_name != NULL && strcmp(arg->short_name, name) == 0) return arg;
        if (arg->long_name != NULL && strcmp(arg->long_name, name) == 0) return arg;
        arg = arg->next;
    }

    return NULL;
}

static bool is_argument(ArgParser* parser, const char* str) {
    /* quick rejection of invalid inputs */
    if (!parser || !str || str[0] == '\0')
        return false;

    /* single-pass through the argument list */
    Argument* arg = parser->arguments;

    while (arg != NULL) {
        /* check short name first */
        if (arg->short_name != NULL && strcmp(arg->short_name, str) == 0)
            return true;

        /* then check long name */
        if (arg->long_name != NULL && strcmp(arg->long_name, str) == 0)
            return true;
        
        arg = arg->next;
    }

    return false;
}

static Argument* find_argument_by_long_name(ArgParser* parser, const char* long_name) {
    /* validate inputs */
    if (!parser || !long_name || long_name[0] == '\0')
        return NULL;

    Argument* arg = parser->arguments;

    while (arg != NULL) {
        if (arg->long_name != NULL && strcmp(arg->long_name, long_name) == 0)
            return arg;
        
        arg = arg->next;
    }

    return NULL;
}

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
    if (!arg || !value) return;
    ListNode** head = (ListNode**)arg->value;
    ListNode** tail_ptr = head;

    /* find the last next pointer */
    while (*tail_ptr)
        tail_ptr = &(*tail_ptr)->next;

    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
    if (!new_node) return;

    new_node->data = value;
    new_node->next = NULL;
    *tail_ptr = new_node;
}

static int list_length(ListNode* head) {
    int count = 0;

    while (head) {
        count++;
        head = head->next;
    }

    return count;
}

/* Helper function to parse multiple values for list arguments. */
static int parse_list_values(ArgParser* parser, Argument* arg,
    int current_index, int argc, char** argv) {
    /* validate inputs */
    if (!parser || !arg || !argv)
        return current_index;

    int i = current_index + 1;
    int values_parsed = 0;

    /* parse all subsequent values until hitting another registered argument */
    while (i < argc && !is_argument(parser, argv[i])) {
        void* value = NULL;

        switch (arg->type) {
        case ARG_INT_LIST: {
            int* val = (int*)malloc(sizeof(int));

            if (val != NULL) {
                *val = atoi(argv[i]);
                value = val;
            }

            break;
        }
        case ARG_DOUBLE_LIST: {
            double* val = (double*)malloc(sizeof(double));

            if (val != NULL) {
                *val = atof(argv[i]);
                value = val;
            }

            break;
        }
        case ARG_STRING_LIST: {
            char* val = strdup(argv[i]);
            if (val) value = val;
            break;
        }
        default:
            /* unknown list type */
            break;
        }

        if (value != NULL) {
            append_to_list(arg, value);
            values_parsed++;
        }
        else {
            /* memory allocation failed */
            fprintf(stderr, "Memory allocation failed for list value.\n");
            break;
        }

        i++;
    }

    if (!values_parsed) {
        fprintf(stderr, "List argument ");
        if (arg->long_name) fprintf(stderr, "%s", arg->long_name);
        else if (arg->short_name) fprintf(stderr, "%s", arg->short_name);
        fprintf(stderr, " requires at least one value\n");
        exit(EXIT_FAILURE);
    }

    arg->set = true;
    return i - 1;
}

ArgParser* argparse_new(const char* description) {
    ArgParser* parser = (ArgParser*)malloc(sizeof(ArgParser));
    if (!parser) return NULL;

    parser->arguments = NULL;
    parser->program_name = NULL;

    parser->description = description ? strdup(description) : NULL;
    parser->help_requested = false;

    /* automatically add help argument */
    argparse_add_argument(parser, "h", "help", ARG_BOOL,
        "Show this help message and exit", false, NULL);
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

void argparse_add_argument(ArgParser* parser, char* short_name, const char* long_name,
    ArgType type, const char* help, bool required, void* default_value) {

    /* validate parser parameter */
    if (!parser) return;

    /* don't add duplicate help arguments, but use exact matching */
    if ((short_name != NULL && strcmp(short_name, "-h") == 0) ||
        (long_name != NULL && strcmp(long_name, "--help") == 0))
        return;

    /* alloc argument structure */
    Argument* arg = (Argument*)malloc(sizeof(Argument));
    if (!arg) return;

    /* init all fields to known state */
    arg->short_name = NULL;
    arg->long_name = NULL;

    arg->help = NULL;
    arg->value = NULL;

    arg->next = NULL;
    arg->required = required;

    arg->set = false;
    arg->type = type;
    arg->is_list = is_list_type(type);

    /* duplicate strings */
    if (short_name) arg->short_name = strdup(short_name);

    if (long_name) arg->long_name = strdup(long_name);
    if (help) arg->help = strdup(help);

    /* handle default value allocation */
    if (default_value) {
        switch (type) {
        case ARG_INT:
            arg->value = (int*)malloc(sizeof(int));
            if (arg->value) *(int*)arg->value = *(int*)default_value;
            break;

        case ARG_DOUBLE:
            arg->value = (double*)malloc(sizeof(double));
            if (arg->value) *(double*)arg->value = *(double*)default_value;
            break;

        case ARG_STRING:
            arg->value = strdup((char*)default_value);
            break;

        case ARG_BOOL:
            arg->value = (bool*)malloc(sizeof(bool));
            if (arg->value) *(bool*)arg->value = *(bool*)default_value;
            break;

        default:
            /* for lists or unknown types */
            arg->value = create_default_value(type);
            break;
        }
    }
    else
        arg->value = create_default_value(type);

    /* check if alloc fails */
    bool allocation_failed = false;

    /* check string allocations */
    if ((short_name && !arg->short_name) ||
        (long_name && !arg->long_name) ||
        (help && !arg->help))
        allocation_failed = true;

    /* check value allocation */
    if (!arg->value && type != ARG_STRING)
        allocation_failed = true;

    /* clean up on failure and return */
    if (allocation_failed) {
        free_argument(arg);
        return;
    }

    /* add to linked list; maintain tail pointer for O(1) appends */
    if (parser->arguments == NULL)
        parser->arguments = arg;
    else {
        /* find tail pointer */
        Argument* tail = parser->arguments;
        while (tail->next) tail = tail->next;
        tail->next = arg;
    }
}

void argparse_add_list_argument(ArgParser* parser, char* short_name, const char* long_name,
    ArgType list_type, const char* help, bool required) {
    argparse_add_argument(parser, short_name, long_name, list_type, help, required, NULL);
}

static bool get_safe_int(const char* str, int* out) {
    char* endptr;
    long val = strtol(str, &endptr, 10);

    /* check for conversion errors */
    if (endptr == str || *endptr != '\0')
        return false;

    /* check for overflow */
    if (val > INT_MAX || val < INT_MIN)
        return false;

    *out = (int)val;
    return true;
}

static bool get_safe_double(const char* str, double* out) {
    char* endptr;
    double val = strtod(str, &endptr);

    /* check for conversion errors */
    if (endptr == str || *endptr != '\0')
        return false;

    /* check for overflow / underflow */
    if (val == HUGE_VAL || val == -HUGE_VAL || val == 0.0) {
        if (errno == ERANGE)
            return false;
    }

    *out = val;
    return true;
}

static void parse_single_value(Argument* arg, const char* str_val) {
    if (!arg || !str_val)
        return;

    /* validate that it is not processing a list argument here */
    if (is_list_type(arg->type)) {
        fprintf(stderr, "Internal error: list argument processed in parse_single_value.\n");
        exit(EXIT_FAILURE);
    }

    /* handle each argument type with proper error checking */
    switch (arg->type) {
    case ARG_INT:
        if (arg->value) {
            int parsed_val = 0;

            if (!get_safe_int(str_val, &parsed_val)) {
                fprintf(stderr, "Invalid integer value: %s.\n", str_val);
                exit(EXIT_FAILURE);
            }

            *(int*)arg->value = parsed_val;
        }
        break;

    case ARG_DOUBLE:
        if (arg->value != NULL) {
            double parsed_val = 0.0;

            if (!get_safe_double(str_val, &parsed_val)) {
                fprintf(stderr, "Invalid floating-point value: %s.\n", str_val);
                exit(EXIT_FAILURE);
            }

            *(double*)arg->value = parsed_val;
        }

        break;

    case ARG_STRING: 
    {
        char* new_value = strdup(str_val);

        if (!new_value) {
            fprintf(stderr, "Memory allocation failed for string value.\n");
            exit(EXIT_FAILURE);
        }

        if (arg->value)
            free(arg->value);

        arg->value = new_value;
    }
    break;

    case ARG_BOOL:
        if (arg->value)
            *(bool*)arg->value = true;
        break;

    default:
        /* unknown argument type */
        fprintf(stderr, "Internal error: unknown argument type in parse_single_value.\n");
        exit(EXIT_FAILURE);
        break;
    }

    arg->set = true;
}

void argparse_parse(ArgParser* parser, int argc, char** argv) {
    if (!parser || argc < 1 || !argv) return;

    if (parser->program_name) free(parser->program_name);
    parser->program_name = strdup(argv[0]);

    /* check if no arguments were provided */
    if (argc == 1) {
        argparse_print_help(parser);
        exit(0);
    }

    for (int i = 1; i < argc; i++) {
        const char* current_arg = argv[i];

        /* check if this is a registered argument */
        if (is_argument(parser, current_arg)) {
            Argument* arg = find_argument(parser, (char*)current_arg);
            if (!arg) arg = find_argument_by_long_name(parser, current_arg);

            if (!arg) {
                /* should not happen since is_argument() returned true */
                fprintf(stderr, "Internal error: Argument not found\n");
                exit(1);
            }

            /* handle help argument */
            if ((arg->short_name && strstr(arg->short_name, "h")) ||
                (arg->long_name && strstr(arg->long_name, "help"))) {
                parser->help_requested = true;
                argparse_print_help(parser);
                exit(0);
            }

            /* found the argument, process it */
            if (arg->type == ARG_BOOL) {
                parse_single_value(arg, "");
            }
            else if (arg->is_list)
                /* handle list arguments with space-separated values */
                i = parse_list_values(parser, arg, i, argc, argv);
            else if (i + 1 < argc) {
                /* check if next token is NOT a registered argument */
                if (!is_argument(parser, argv[i + 1]))
                    /* next token is a value */
                    parse_single_value(arg, argv[++i]);
                else {
                    /* next token is another argument */
                    fprintf(stderr, "Option ");

                    if (arg->short_name) fprintf(stderr, "%s", arg->short_name);
                    else if (arg->long_name) fprintf(stderr, "%s", arg->long_name);

                    fprintf(stderr, " requires a value\n");
                    exit(1);
                }
            }
            else {
                /* no more arguments, but this one needs a value */
                fprintf(stderr, "Option ");

                if (arg->short_name) fprintf(stderr, "%s", arg->short_name);
                else if (arg->long_name) fprintf(stderr, "%s", arg->long_name);

                fprintf(stderr, " requires a value\n");
                exit(1);
            }
        }
        else {
            /* not a registered argument - it's a value without preceding option */
            fprintf(stderr, "Unexpected value: %s (did you forget an option?)\n", current_arg);
            exit(1);
        }
    }

    /* validate required arguments */
    Argument* current = parser->arguments;

    while (current) {
        if (current->required && !current->set) {
            fprintf(stderr, "Required argument ");

            if (current->short_name) fprintf(stderr, "%s", current->short_name);
            else if (current->long_name) fprintf(stderr, "%s", current->long_name);

            fprintf(stderr, " not provided\n");
            exit(1);
        }

        current = current->next;
    }
}

/* value access implementations */
bool argparse_get_bool(ArgParser* parser, char* name) {
    Argument* arg = find_argument(parser, name);
    return arg && arg->set ? *(bool*)arg->value : false;
}

int argparse_get_int(ArgParser* parser, char* name) {
    Argument* arg = find_argument(parser, name);
    return arg && arg->set ? *(int*)arg->value : 0;
}

double argparse_get_double(ArgParser* parser, char* name) {
    Argument* arg = find_argument(parser, name);
    return arg && arg->set ? *(double*)arg->value : 0.0;
}

const char* argparse_get_string(ArgParser* parser, char* name) {
    Argument* arg = find_argument(parser, name);
    return arg && arg->set ? (const char*)arg->value : NULL;
}

/* list access implementations */
int argparse_get_list_count(ArgParser* parser, char* name) {
    Argument* arg = find_argument(parser, name);
    if (!arg || !arg->set) return 0;

    ListNode* head = *(ListNode**)arg->value;
    return list_length(head);
}

int argparse_get_int_list(ArgParser* parser, char* name, int** values) {
    Argument* arg = find_argument(parser, name);
    if (!arg || !arg->set || arg->type != ARG_INT_LIST) return 0;
    ListNode* head = *(ListNode**)arg->value;

    int count = list_length(head);
    if (count == 0) return 0;

    *values = (int*)malloc(count * sizeof(int));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for int list.\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        (*values)[i] = *(int*)current->data;
        current = current->next;
    }

    return count;
}

int argparse_get_double_list(ArgParser* parser, char* name, double** values) {
    Argument* arg = find_argument(parser, name);
    if (!arg || !arg->set || arg->type != ARG_DOUBLE_LIST) return 0;

    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);
    if (count == 0) return 0;

    *values = (double*)malloc(count * sizeof(double));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for double list.\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        (*values)[i] = *(double*)current->data;
        current = current->next;
    }

    return count;
}

int argparse_get_string_list(ArgParser* parser, char* name, char*** values) {
    Argument* arg = find_argument(parser, name);
    if (!arg || !arg->set || arg->type != ARG_STRING_LIST) return 0;

    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);
    if (count == 0) return 0;

    *values = (char**)malloc(count * sizeof(char*));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for string list.\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        (*values)[i] = strdup((char*)current->data);
        current = current->next;
    }

    return count;
}

/* fixed list freeing functions */
void argparse_free_int_list(int** values, int count) {
    if (!values || !*values) return;
    free(*values);
    *values = NULL;
}

void argparse_free_double_list(double** values, int count) {
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
    printf("Usage: %s [OPTIONS]\n\n", parser->program_name);
    if (parser->description) printf("%s\n\n", parser->description);

    Argument* current = parser->arguments;

    while (current) {
        printf("  ");
        if (current->short_name)
            printf("%s", current->short_name);

        if (current->long_name) {
            if (current->short_name) printf(", ");
            printf("%s", current->long_name);
        }

        switch (current->type) {
        case ARG_INT:
        case ARG_DOUBLE:
        case ARG_STRING:
            printf(" VALUE");
            break;
        case ARG_INT_LIST:
        case ARG_DOUBLE_LIST:
        case ARG_STRING_LIST:
            /* updated help format */
            printf(" VALUE1 VALUE2 ...");
            break;
        default:
            break;
        }

        printf("\n    %s", current->help ? current->help : "");
        if (current->required) printf(" [required]");

        printf("\n");
        current = current->next;
    }
}

void argparse_free(ArgParser* parser) {
    if (!parser) return;
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