#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Return a null-terminated duplicate of the string referenced by str. */
char* ap_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;

    char* copy = malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* Internal list node structure */
typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

struct Argument {
    char short_name;
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

static Argument* find_argument(ArgParser* parser, char short_name) {
    Argument* current = parser->arguments;

    while (current) {
        if (current->short_name == short_name)
            return current;

        current = current->next;
    }

    return NULL;
}

static Argument* find_argument_by_long_name(ArgParser* parser, const char* long_name) {
    Argument* current = parser->arguments;

    while (current) {
        if (current->long_name && strcmp(current->long_name, long_name) == 0)
            return current;

        current = current->next;
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
        ListNode** list = malloc(sizeof(ListNode*));
        if (!list) return NULL;

        *list = NULL;
        return list;
    }
    default:
        return NULL;
    }
}

static void append_to_list(Argument* arg, void* value) {
    ListNode** head = (ListNode**)arg->value;
    ListNode* new_node = malloc(sizeof(ListNode));

    if (!new_node) {
        fprintf(stderr, "Memory allocation failed for list node\n");
        exit(1);
    }

    new_node->data = value;
    new_node->next = NULL;

    if (*head == NULL)
        *head = new_node;
    else {
        ListNode* current = *head;

        while (current->next)
            current = current->next;

        current->next = new_node;
    }
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
static int parse_list_values(ArgParser* parser, Argument* arg, int current_index, int argc, char** argv) {
    int i = current_index + 1;
    int values_parsed = 0;

    /* parse all subsequent non-option arguments as list values */
    while (i < argc && argv[i][0] != '-') {
        switch (arg->type) {
        case ARG_INT_LIST: {
            int* val = (int*)malloc(sizeof(int));
            if(val) *val = atoi(argv[i]);

            append_to_list(arg, val);
            break;
        }
        case ARG_DOUBLE_LIST: {
            double* val = (double*)malloc(sizeof(double));
            if(val) *val = atof(argv[i]);

            append_to_list(arg, val);
            break;
        }
        case ARG_STRING_LIST: {
            char* val = strdup(argv[i]);
            append_to_list(arg, val);
            break;
        }
        default:
            /* unknown list type */
            break;
        }

        values_parsed++;
        i++;
    }

    if (values_parsed > 0)
        arg->set = true;
    else {
        fprintf(stderr, "List argument ");
        if (arg->long_name) fprintf(stderr, "--%s", arg->long_name);
        else fprintf(stderr, "-%c", arg->short_name);
        fprintf(stderr, " requires at least one value\n");
        exit(1);
    }

    /* return the last index we processed */
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
    argparse_add_argument(parser, 'h', "help", ARG_BOOL, "Show this help message and exit", false, NULL);
    return parser;
}

static void free_argument(Argument* arg) {
    if (!arg) return;

    free(arg->long_name);
    free(arg->help);

    switch (arg->type) {
    case ARG_INT:
    case ARG_DOUBLE:
    case ARG_BOOL:
        free(arg->value);
        break;
    case ARG_STRING:
        if (arg->value) free(arg->value);
        break;
    case ARG_INT_LIST:
    case ARG_DOUBLE_LIST:
    case ARG_STRING_LIST: {
        ListNode* head = *(ListNode**)arg->value;

        while (head) {
            ListNode* next = head->next;
            free(head->data);

            free(head);
            head = next;
        }

        free(arg->value);
        break;
    }
    }

    free(arg);
}

void argparse_add_argument(ArgParser* parser, char short_name, const char* long_name,
    ArgType type, const char* help, bool required, void* default_value) {

    /* don't add duplicate help arguments */
    if (short_name == 'h' || (long_name && strcmp(long_name, "help") == 0))
        return;

    Argument* arg = malloc(sizeof(Argument));
    if (!arg) return;

    arg->short_name = short_name;
    arg->long_name = long_name ? strdup(long_name) : NULL;

    arg->help = help ? strdup(help) : NULL;
    arg->type = type;

    arg->required = required;
    arg->set = false;

    arg->is_list = is_list_type(type);
    arg->next = NULL;

    if (default_value) {
        switch (type) {
        case ARG_INT:
            arg->value = malloc(sizeof(int));
            if (arg->value) *(int*)arg->value = *(int*)default_value;
            break;
        case ARG_DOUBLE:
            arg->value = malloc(sizeof(double));
            if (arg->value) *(double*)arg->value = *(double*)default_value;
            break;
        case ARG_STRING:
            arg->value = strdup((char*)default_value);
            break;
        case ARG_BOOL:
            arg->value = malloc(sizeof(bool));
            if (arg->value) *(bool*)arg->value = *(bool*)default_value;
            break;
        default:
            arg->value = create_default_value(type);
            break;
        }
    }
    else {
        arg->value = create_default_value(type);
    }

    /* Check if memory allocation for value failed */
    if (!arg->value && type != ARG_STRING) {
        free_argument(arg);
        return;
    }

    /* add to linked list */
    if (!parser->arguments)
        parser->arguments = arg;
    else {
        Argument* current = parser->arguments;
        while (current->next) current = current->next;
        current->next = arg;
    }
}

void argparse_add_list_argument(ArgParser* parser, char short_name, const char* long_name,
    ArgType list_type, const char* help, bool required) {
    argparse_add_argument(parser, short_name, long_name, list_type, help, required, NULL);
}

static void parse_single_value(Argument* arg, const char* str_val) {
    /* 
        list arguments are now handled separately in parse_list_values
        this function only handles single-value arguments
    */

    switch (arg->type) {
    case ARG_INT:
        *(int*)arg->value = atoi(str_val);
        break;
    case ARG_DOUBLE:
        *(double*)arg->value = atof(str_val);
        break;
    case ARG_STRING:
        if (arg->value) free(arg->value);
        arg->value = strdup(str_val);
        break;
    case ARG_BOOL:
        *(bool*)arg->value = true;
        break;
    case ARG_INT_LIST:
    case ARG_DOUBLE_LIST:
    case ARG_STRING_LIST:
        /* these should not reach here - list arguments are handled in parse_list_values */
        fprintf(stderr, "Internal error: list argument processed in parse_single_value\n");
        exit(1);
        break;
    }
    arg->set = true;
}

void argparse_parse(ArgParser* parser, int argc, char** argv) {
    if (!parser || argc < 1 || !argv) return;
    parser->program_name = strdup(argv[0]);

    /* check if no arguments were provided (only program name) */
    if (argc == 1) {
        argparse_print_help(parser);
        exit(0);
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                /* long option */
                char* option = argv[i] + 2;

                /* check for help option first */
                if (strcmp(option, "help") == 0) {
                    parser->help_requested = true;
                    argparse_print_help(parser);
                    exit(0);
                }

                char* equals = strchr(option, '=');
                Argument* arg = NULL;

                if (equals) {
                    *equals = '\0';
                    arg = find_argument_by_long_name(parser, option);
                    if (arg) {
                        if (arg->is_list) {
                            fprintf(stderr, "List argument --%s cannot use '=' syntax\n", option);
                            exit(1);
                        }
                        parse_single_value(arg, equals + 1);
                    }
                }
                else {
                    arg = find_argument_by_long_name(parser, option);
                    if (arg) {
                        if (arg->type == ARG_BOOL) {
                            parse_single_value(arg, "");
                        }
                        else if (arg->is_list) {
                            /* handle list arguments with space-separated values */
                            i = parse_list_values(parser, arg, i, argc, argv);
                        }
                        else if (i + 1 < argc)
                            parse_single_value(arg, argv[++i]);
                        else {
                            fprintf(stderr, "Option --%s requires a value\n", option);
                            exit(1);
                        }
                    }
                }

                if (!arg) {
                    fprintf(stderr, "Unknown option: --%s\n", option);
                    exit(1);
                }
            }
            else {
                /* short option */
                char short_name = argv[i][1];

                /* check for help option first */
                if (short_name == 'h') {
                    parser->help_requested = true;
                    argparse_print_help(parser);
                    exit(0);
                }

                Argument* arg = find_argument(parser, short_name);
                if (!arg) {
                    fprintf(stderr, "Unknown option: -%c\n", short_name);
                    exit(1);
                }

                if (arg->type == ARG_BOOL) {
                    parse_single_value(arg, "");
                }
                else if (arg->is_list) {
                    /* handle list arguments with space-separated values */
                    i = parse_list_values(parser, arg, i, argc, argv);
                }
                else if (i + 1 < argc) {
                    parse_single_value(arg, argv[++i]);
                }
                else {
                    fprintf(stderr, "Option -%c requires a value\n", short_name);
                    exit(1);
                }
            }
        }
        else {
            fprintf(stderr, "Unexpected positional argument: %s\n", argv[i]);
            exit(1);
        }
    }

    /* validate required arguments */
    Argument* current = parser->arguments;
    while (current) {
        if (current->required && !current->set) {
            fprintf(stderr, "Required argument ");
            if (current->long_name) fprintf(stderr, "--%s", current->long_name);
            else fprintf(stderr, "-%c", current->short_name);
            fprintf(stderr, " not provided\n");
            exit(1);
        }
        current = current->next;
    }
}

/* value access implementations */
bool argparse_get_bool(ArgParser* parser, char short_name) {
    Argument* arg = find_argument(parser, short_name);
    return arg && arg->set ? *(bool*)arg->value : false;
}

int argparse_get_int(ArgParser* parser, char short_name) {
    Argument* arg = find_argument(parser, short_name);
    return arg && arg->set ? *(int*)arg->value : 0;
}

double argparse_get_double(ArgParser* parser, char short_name) {
    Argument* arg = find_argument(parser, short_name);
    return arg && arg->set ? *(double*)arg->value : 0.0;
}

const char* argparse_get_string(ArgParser* parser, char short_name) {
    Argument* arg = find_argument(parser, short_name);
    return arg && arg->set ? (const char*)arg->value : NULL;
}

/* list access implementations */
int argparse_get_list_count(ArgParser* parser, char short_name) {
    Argument* arg = find_argument(parser, short_name);
    if (!arg || !arg->set) return 0;

    ListNode* head = *(ListNode**)arg->value;
    return list_length(head);
}

int argparse_get_int_list(ArgParser* parser, char short_name, int** values) {
    Argument* arg = find_argument(parser, short_name);
    if (!arg || !arg->set || arg->type != ARG_INT_LIST) return 0;

    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);
    if (count == 0) return 0;

    *values = (int*)malloc(count * sizeof(int));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for int list\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        (*values)[i] = *(int*)current->data;
        current = current->next;
    }

    return count;
}

int argparse_get_double_list(ArgParser* parser, char short_name, double** values) {
    Argument* arg = find_argument(parser, short_name);
    if (!arg || !arg->set || arg->type != ARG_DOUBLE_LIST) return 0;

    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);
    if (count == 0) return 0;

    *values = (double*)malloc(count * sizeof(double));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for double list\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        (*values)[i] = *(double*)current->data;
        current = current->next;
    }

    return count;
}

int argparse_get_string_list(ArgParser* parser, char short_name, char*** values) {
    Argument* arg = find_argument(parser, short_name);
    if (!arg || !arg->set || arg->type != ARG_STRING_LIST) return 0;

    ListNode* head = *(ListNode**)arg->value;
    int count = list_length(head);
    if (count == 0) return 0;

    *values = (char**)malloc(count * sizeof(char*));
    ListNode* current = head;

    if (!*values) {
        fprintf(stderr, "Memory allocation failed for string list\n");
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
        if (current->short_name) printf("-%c", current->short_name);
        if (current->long_name) {
            if (current->short_name) printf(", ");
            printf("--%s", current->long_name);
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