#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int compare_ints(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

static void cleanup_and_exit(ArgParser* parser, int* numbers, int exit_code) {
    if (numbers) argparse_free_int_list(&numbers);
    if (parser) argparse_free(parser);
    exit(exit_code);
}

int main(int argc, char** argv) {
    ArgParser* parser = NULL;
    int* numbers = NULL;

    /* create argument parser */
    parser = argparse_new("Advanced number statistics calculator.");
    if (!parser) {
        fprintf(stderr, "Fatal error: Failed to initialize argument parser.\n");
        return EXIT_FAILURE;
    }

    /* add arguments with some default values */
    int default_round = 2;
    argparse_add_argument(parser, "-r", "--round", ARG_INT,
        "Number of decimal places for output", false, &default_round);

    argparse_add_list_argument(parser, "-n", "--numbers", ARG_INT_LIST,
        "List of integers for calculation", true);

    argparse_add_argument(parser, "-a", "--average", ARG_BOOL,
        "Calculate mean average", false, NULL);

    argparse_add_argument(parser, "-m", "--median", ARG_BOOL,
        "Calculate median", false, NULL);

    argparse_add_argument(parser, "-s", "--stats", ARG_BOOL,
        "Show all statistics", false, NULL);

    argparse_add_argument(parser, "-v", "--verbose", ARG_BOOL,
        "Detailed output", false, NULL);

    /* check for argument definition errors */
    if (argparse_error_occurred()) {
        fprintf(stderr, "Argument configuration error: %s.\n",
            argparse_get_last_error_message());
        cleanup_and_exit(parser, NULL, EXIT_FAILURE);
    }

    /* parse arguments */
    argparse_parse(parser, argc, argv);

    /* handle parse errors */
    if (argparse_error_occurred()) {
        int err = argparse_get_last_error();
        const char* err_msg = argparse_get_last_error_message();

        if (err == 0 && err_msg[0] != '\0') {
            /* Help requested or no arguments - exit cleanly */
            argparse_clear_error();
            cleanup_and_exit(parser, NULL, EXIT_SUCCESS);
        }
        else {
            /* Actual error occurred */
            fprintf(stderr, "Parse error: %s.\n", err_msg);
            cleanup_and_exit(parser, NULL, EXIT_FAILURE);
        }
    }

    /* get the numbers list */
    int count = argparse_get_int_list(parser, "-n", &numbers);

    /* check for retrieval errors */
    if (argparse_error_occurred()) {
        fprintf(stderr, "Data retrieval error: %s.\n",
            argparse_get_last_error_message());
        argparse_clear_error();
        cleanup_and_exit(parser, numbers, EXIT_FAILURE);
    }

    if (count == 0) {
        fprintf(stderr, "Error: No numbers provided (list count is zero).\n");
        fprintf(stderr, "Use -n or --numbers to provide integers.\n");
        cleanup_and_exit(parser, numbers, EXIT_FAILURE);
    }

    /* validate we have data */
    if (!numbers) {
        fprintf(stderr, "Fatal error: Number list pointer is NULL.\n");
        cleanup_and_exit(parser, NULL, EXIT_FAILURE);
    }

    /* get rounding precision */
    int decimals = argparse_get_int(parser, "-r");

    /* clamp decimals to reasonable range */
    if (decimals < 0) {
        fprintf(stderr, "Warning: Negative decimal places (%d) not allowed, using 0.\n", decimals);
        decimals = 0;
    }
    else if (decimals > 10) {
        fprintf(stderr, "Warning: Excessive decimal places (%d) capped at 10.\n", decimals);
        decimals = 10;
    }

    bool show_average = argparse_get_bool(parser, "-a");
    bool show_median = argparse_get_bool(parser, "-m");
    bool show_stats = argparse_get_bool(parser, "-s");
    bool verbose = argparse_get_bool(parser, "-v");

    /* if no specific operation requested, default to average */
    if (!show_average && !show_median && !show_stats)
        show_average = true;

    /* calculate basic statistics */
    double sum = 0.0;
    int min = numbers[0];
    int max = numbers[0];

    for (int i = 0; i < count; i++) {
        sum += numbers[i];
        if (numbers[i] < min) min = numbers[i];
        if (numbers[i] > max) max = numbers[i];
    }

    double average = sum / count;

    /* calculate median if requested */
    double median = 0.0;

    if (show_median || show_stats) {
        /* create sorted copy to preserve original data */
        int* sorted_numbers = (int*)malloc((size_t)count * sizeof(int));
        if (!sorted_numbers) {
            fprintf(stderr, "Memory error: Failed to allocate sorted array.\n");
            cleanup_and_exit(parser, numbers, EXIT_FAILURE);
        }
        else {
            memcpy(sorted_numbers, numbers, (size_t)count * sizeof(int));
            qsort(sorted_numbers, (size_t)count, sizeof(int), compare_ints);

            if (count % 2 == 0) median = (sorted_numbers[count / 2 - 1] + sorted_numbers[count / 2]) / 2.0;
            else median = sorted_numbers[count / 2];
            free(sorted_numbers);
        }
    }

    /* display results based on flags */
    if (verbose) {
        printf("Input numbers: ");

        for (int i = 0; i < count; i++) {
            printf("%d", numbers[i]);
            if (i < count - 1) printf(", ");
        }

        printf("\n");
        printf("Count: %d\n", count);
        printf("Decimals: %d\n", decimals);
    }

    if (show_average || show_stats)
        printf("Average: %.*f\n", decimals, average);

    if (show_median || show_stats)
        printf("Median: %.*f\n", decimals, median);

    if (show_stats) {
        printf("Minimum: %d\n", min);
        printf("Maximum: %d\n", max);
        printf("Range: %d\n", max - min);
        printf("Sum: %.*f\n", decimals, sum);
    }

    /* clean up and exit */
    cleanup_and_exit(parser, numbers, EXIT_SUCCESS);

    /* unreachable, but included for completeness */
    return EXIT_SUCCESS;
}