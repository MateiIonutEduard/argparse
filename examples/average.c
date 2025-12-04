#include "argparse.h"
#include <stdio.h>

int main(int argc, char** argv) {
    /* create argument parser */
    ArgParser* parser = argparse_new("Calculate average of a list of integers.");

    /* add arguments */
    argparse_add_argument(parser, "-a", "--average", ARG_BOOL, "Calculate and display the average", false, NULL);
    argparse_add_list_argument(parser, "-n", "--numbers", ARG_INT_LIST, "List of integers to average", false);
    argparse_add_argument(parser, "-v", "--verbose", ARG_BOOL, "Show detailed output", false, NULL);

    /* parse command line arguments */
    argparse_parse(parser, argc, argv);
    
    /* check if average calculation was requested */
    if (argparse_get_bool(parser, "-a")) {
        int* numbers = NULL;
        int count = argparse_get_int_list(parser, "-n", &numbers);

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

                printf("\n");
                printf("count: %d\n", count);

                printf("sum: %.2f\n", sum);
                printf("average: %.2f\n", average);
            }
            else
                /* simple output */
                printf("average: %.2f\n", average);

            /* free the list memory */
            argparse_free_int_list(&numbers, count);
        }
        else {
            printf("Error: No numbers provided for average calculation.\n");
            printf("Use -n or --numbers to provide integers.\n");
        }
    }
    else {
        /* if average flag wasn't provided, show usage hint */
        printf("Use --average or -a flag to calculate average of numbers.\n");

        /* program name is stored elsewhere */
        printf("Example: %s --numbers 10 20 30 40 --average --verbose\n",
            argv[0]);
    }

    /* clean up */
    argparse_free(parser);
    return 0;
}
