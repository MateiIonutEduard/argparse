#include "argparse_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Simplified thread-local error state. */
#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

/* Thread-local error state. */
static THREAD_LOCAL ArgParseError g_last_error = { 0 };
static THREAD_LOCAL char g_message_buffer[512] = { 0 };
static THREAD_LOCAL bool g_error_occurred = false;

void argparse_error_set(ArgParseErrorCategory category, int errno_val,
    const char* func, int line, const char* arg_name, const char* user_msg) {
    /* set errno for better compatibility */
    errno = errno_val;

    g_last_error.category = category;
    g_last_error.errno_value = errno_val;

    g_last_error.function_name = func;
    g_last_error.line_number = line;

    g_last_error.argument_name = arg_name ? arg_name : "";
    g_last_error.user_message = user_msg ? user_msg : "";

    /* format full message */
    const char* category_str = argparse_error_category_to_string(category);

    if (user_msg && user_msg[0] != '\0') {
        if (arg_name && arg_name[0] != '\0') {
            snprintf(g_message_buffer, sizeof(g_message_buffer),
                "[%s] Argument '%s': %s.", category_str, arg_name, user_msg);
        }
        else {
            snprintf(g_message_buffer, sizeof(g_message_buffer),
                "[%s] %s.", category_str, user_msg);
        }
    }
    else {
        if (arg_name && arg_name[0] != '\0') {
            snprintf(g_message_buffer, sizeof(g_message_buffer),
                "[%s] Argument '%s'.", category_str, arg_name);
        }
        else {
            snprintf(g_message_buffer, sizeof(g_message_buffer),
                "[%s]", category_str);
        }
    }

    g_error_occurred = true;
}

void argparse_error_clear(void) {
    memset(&g_last_error, 0, sizeof(ArgParseError));
    g_message_buffer[0] = '\0';

    g_error_occurred = false;
    errno = 0;
}

ArgParseErrorCategory argparse_error_get_category(void) {
    return g_last_error.category;
}

int argparse_error_get_errno(void) {
    return g_last_error.errno_value;
}

const char* argparse_error_get_message(void) {
    return g_message_buffer;
}

const char* argparse_error_get_argument(void) {
    return g_last_error.argument_name;
}

bool argparse_error_occurred(void) {
    return g_error_occurred;
}

bool argparse_error_is_fatal(void) {
    switch (g_last_error.category) {
    case APE_HELP_REQUESTED:
        return false;
    case APE_SUCCESS:
        return false;
    default:
        return true;
    }
}

const char* argparse_error_category_to_string(ArgParseErrorCategory category) {
    static const char* category_strings[] = {
        [APE_SUCCESS] = "SUCCESS",
        [APE_MEMORY] = "MEMORY_ERROR",
        [APE_SYNTAX] = "SYNTAX_ERROR",
        [APE_TYPE] = "TYPE_ERROR",
        [APE_REQUIRED] = "REQUIRED_ERROR",
        [APE_VALIDATION] = "VALIDATION_ERROR",
        [APE_INTERNAL] = "INTERNAL_ERROR",
        [APE_CONFIG] = "CONFIG_ERROR",
        [APE_RANGE] = "RANGE_ERROR",
        [APE_UNKNOWN_ARG] = "UNKNOWN_ARGUMENT",
        [APE_DUPLICATE] = "DUPLICATE_ARGUMENT",
        [APE_HELP_REQUESTED] = "HELP_REQUESTED"
    };

    if (category >= 0 && category <= APE_HELP_REQUESTED)
        return category_strings[category];
    return "UNKNOWN_CATEGORY";
}

void argparse_error_cleanup(void) {
    argparse_error_clear();
}