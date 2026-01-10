#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    /* create argument parser */
    ArgParser* parser = argparse_new("Calculate average of a list of integers.");
    if (!parser) {
        fprintf(stderr, "Fatal error: Failed to initialize argument parser\n");
        return EXIT_FAILURE;
    }

    /* add arguments */
    argparse_add_argument(parser, "-a", "--average", ARG_BOOL,
        "Calculate and display the average", false, NULL);
    argparse_add_list_argument(parser, "-n", "--numbers", ARG_INT_LIST,
        "List of integers to average", false);
    argparse_add_argument(parser, "-v", "--verbose", ARG_BOOL,
        "Show detailed output", false, NULL);

    /* check for argument definition errors */
    if (argparse_error_occurred()) {
        fprintf(stderr, "Argument configuration error: %s.\n",
            argparse_get_last_error_message());
        argparse_free(parser);
        return EXIT_FAILURE;
    }

    /* parse command line arguments */
    argparse_parse(parser, argc, argv);

    /* handle help request (non-fatal error) */
    if (argparse_get_last_error() != 0) {
        int err = argparse_get_last_error();
        const char* err_msg = argparse_get_last_error_message();

        if (err == 0 && err_msg[0] != '\0') {
            /* help requested or no arguments, exit cleanly */
            argparse_clear_error();
            argparse_free(parser);
            return EXIT_SUCCESS;
        }
        else {
            /* actual error occurred */
            fprintf(stderr, "Parse error: %s (errno=%d).\n", err_msg, err);
            argparse_free(parser);
            return EXIT_FAILURE;
        }
    }

    /* check if average calculation was requested */
    if (argparse_get_bool(parser, "-a")) {
        int* numbers = NULL;
        int count = argparse_get_int_list(parser, "-n", &numbers);

        /* check for retrieval errors */
        if (argparse_error_occurred()) {
            fprintf(stderr, "Data retrieval error: %s.\n",
                argparse_get_last_error_message());
            argparse_clear_error();
            argparse_free_int_list(&numbers);
            argparse_free(parser);
            return EXIT_FAILURE;
        }

        if (count > 0) {
            /* calculate average */
            double sum = 0.0;
            for (int i = 0; i < count; i++)
                sum += numbers[i];

            double average = sum / count;

            /* display results */
            if (argparse_get_bool(parser, "-v")) {
                /* verbose output */
                printf("Numbers provided: ");
                for (int i = 0; i < count; i++) {
                    printf("%d", numbers[i]);
                    if (i < count - 1) printf(", ");
                }
                printf("\nCount: %d\n", count);
                printf("Sum: %.2f\n", sum);
                printf("Average: %.2f\n", average);
            }
            else {
                /* simple output */
                printf("Average: %.2f\n", average);
            }

            /* free the list memory */
            argparse_free_int_list(&numbers);
        }
        else {
            printf("Error: No numbers provided for average calculation.\n");
            printf("Use -n or --numbers to provide integers.\n");
        }
    }
    else {
        /* if average flag wasn't provided, show usage hint */
        printf("Use --average or -a flag to calculate average of numbers.\n");
        printf("Example: %s --numbers 10 20 30 40 --average --verbose\n", argv[0]);
    }

    /* clean up */
    argparse_free(parser);
    return EXIT_SUCCESS;
}