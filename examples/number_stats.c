#include "argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char** argv) {
    ArgParser* parser = argparse_new("Advanced number statistics calculator.");
    
    /* add arguments with some default values */
    int default_round = 2;
    argparse_add_argument(parser, "-r", "--round", ARG_INT, 
                         "Number of decimal places for output", false, &default_round);
    
    argparse_add_list_argument(parser, "-n", "--numbers", ARG_INT_LIST, 
                              "List of integers for calculation", true); // required
    
    argparse_add_argument(parser, "-a", "--average", ARG_BOOL, 
                         "Calculate mean average", false, NULL);
    
    argparse_add_argument(parser, "-m", "--median", ARG_BOOL, 
                         "Calculate median", false, NULL);
    
    argparse_add_argument(parser, "-s", "--stats", ARG_BOOL, 
                         "Show all statistics", false, NULL);
    
    argparse_add_argument(parser, "-v", "--verbose", ARG_BOOL, 
                         "Detailed output", false, NULL);
    
    /* parse arguments */
    argparse_parse(parser, argc, argv);
    
    /* get the numbers list */
    int* numbers = NULL;
    int count = argparse_get_int_list(parser, "-n", &numbers);
    
    if (count == 0) {
        printf("Error: No numbers provided.\n");
        argparse_free(parser);
        return 1;
    }
    
    /* get rounding precision */
    int decimals = argparse_get_int(parser, "-r");
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
        /* sort the numbers (simple bubble sort for demonstration) */
        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                if (numbers[j] > numbers[j + 1]) {
                    int temp = numbers[j];
                    numbers[j] = numbers[j + 1];
                    numbers[j + 1] = temp;
                }
            }
        }
        
        if (count % 2 == 0)
            median = (numbers[count / 2 - 1] + numbers[count / 2]) / 2.0;
        else
            median = numbers[count / 2];
    }
    
    /* display results based on flags */
    if (verbose) {
        printf("Input numbers: ");
		
        for (int i = 0; i < count; i++) {
            printf("%d", numbers[i]);
            if (i < count - 1) printf(", ");
        }
		
        printf("\n");
        printf("count: %d\n", count);
    }
    
    if (show_average || show_stats)
        printf("average: %.*f\n", decimals, average);
    
    if (show_median || show_stats)
        printf("median: %.*f\n", decimals, median);
    
    if (show_stats) {
        printf("min: %d\n", min);
        printf("max: %d\n", max);
		
        printf("range: %d\n", max - min);
        printf("sum: %.*f\n", decimals, sum);
    }
    
    /* free memory */
    argparse_free_int_list(&numbers, count);
	
    argparse_free(parser);
    return 0;
}