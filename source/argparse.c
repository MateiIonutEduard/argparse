#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* internal list node structure */
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
    Argument* next;
};

struct ArgParser {
    Argument* arguments;
    char* program_name;
    char* description;
};

/* internal utility functions */
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

static void* create_default_value(ArgType type) {
    switch (type) {
    case ARG_INT: {
        int* val = malloc(sizeof(int));
        *val = 0;
        return val;
    }
    case ARG_DOUBLE: {
        double* val = malloc(sizeof(double));
        *val = 0.0;
        return val;
    }
    case ARG_STRING:
        return NULL;
    case ARG_BOOL: {
        bool* val = malloc(sizeof(bool));
        *val = false;
        return val;
    }
    case ARG_INT_LIST:
    case ARG_DOUBLE_LIST:
    case ARG_STRING_LIST: {
        ListNode** list = malloc(sizeof(ListNode*));
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

/* public API implementation */
ArgParser* argparse_new(const char* description) {
    ArgParser* parser = malloc(sizeof(ArgParser));
    if (!parser) return NULL;

    parser->arguments = NULL;
    parser->program_name = NULL;

    parser->description = description ? strdup(description) : NULL;
    return parser;
}

void argparse_add_argument(ArgParser* parser, char short_name, const char* long_name,
    ArgType type, const char* help, bool required, void* default_value) {
    Argument* arg = malloc(sizeof(Argument));
    if (!arg) return;

    arg->short_name = short_name;
    arg->long_name = long_name ? strdup(long_name) : NULL;

    arg->help = help ? strdup(help) : NULL;
    arg->type = type;

    arg->required = required;
    arg->set = false;
    arg->next = NULL;

    if (default_value) {
        switch (type) {
        case ARG_INT:
            arg->value = malloc(sizeof(int));
            *(int*)arg->value = *(int*)default_value;
            break;
        case ARG_DOUBLE:
            arg->value = malloc(sizeof(double));
            *(double*)arg->value = *(double*)default_value;
            break;
        case ARG_STRING:
            arg->value = strdup((char*)default_value);
            break;
        case ARG_BOOL:
            arg->value = malloc(sizeof(bool));
            *(bool*)arg->value = *(bool*)default_value;
            break;
        default:
            arg->value = create_default_value(type);
            break;
        }
    }
    else {
        arg->value = create_default_value(type);
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
    case ARG_INT_LIST: {
        int* val = malloc(sizeof(int));
        *val = atoi(str_val);
        append_to_list(arg, val);
        break;
    }
    case ARG_DOUBLE_LIST: {
        double* val = malloc(sizeof(double));
        *val = atof(str_val);
        append_to_list(arg, val);
        break;
    }
    case ARG_STRING_LIST: {
        char* val = strdup(str_val);
        append_to_list(arg, val);
        break;
    }
    }
    arg->set = true;
}

void argparse_parse(ArgParser* parser, int argc, char** argv) {
    if (!parser || argc < 1 || !argv) return;
    parser->program_name = strdup(argv[0]);

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                /* long option */
                char* option = argv[i] + 2;

                char* equals = strchr(option, '=');
                Argument* arg = NULL;

                if (equals) {
                    *equals = '\0';
                    arg = find_argument_by_long_name(parser, option);
                    if (arg) parse_single_value(arg, equals + 1);
                }
                else {
                    arg = find_argument_by_long_name(parser, option);

                    if (arg) {
                        if (arg->type == ARG_BOOL)
                            parse_single_value(arg, "");
                        else if (i + 1 < argc)
                            parse_single_value(arg, argv[++i]);
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
                Argument* arg = find_argument(parser, short_name);

                if (!arg) {
                    fprintf(stderr, "Unknown option: -%c\n", short_name);
                    exit(1);
                }

                if (arg->type == ARG_BOOL)
                    parse_single_value(arg, "");
                else if (i + 1 < argc)
                    parse_single_value(arg, argv[++i]);
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

    *values = malloc(count * sizeof(int));
    ListNode* current = head;
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

    *values = malloc(count * sizeof(double));
    ListNode* current = head;
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

    *values = malloc(count * sizeof(char*));
    ListNode* current = head;
    for (int i = 0; i < count; i++) {
        (*values)[i] = strdup((char*)current->data);
        current = current->next;
    }
    return count;
}

/* fixed list freeing functions - no more void** issues */
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
            printf(" VALUE [VALUE...]");
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