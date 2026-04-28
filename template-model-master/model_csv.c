#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ross.h"
#include "model.h"

void csv_load(const char* path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("ERROR: Could not open dataset at %s\n", path);
        exit(1);
    }

    // For this simulation, we are modeling 1 core asset (the S&P 500 index)
    g_num_stocks = 1;
    g_stocks = malloc(sizeof(stock_data) * g_num_stocks);
    g_stocks[0].stock_id = 0;

    // 1. Count the number of lines to allocate exact memory
    char line[1024];
    size_t count = 0;
    
    fgets(line, sizeof(line), fp); // Skip the header row
    while (fgets(line, sizeof(line), fp)) {
        count++;
    }

    g_stocks[0].count = count;
    g_stocks[0].rows = malloc(sizeof(csv_row) * count);

    // 2. Rewind the file and actually parse the data
    rewind(fp);
    fgets(line, sizeof(line), fp); // Skip the header row again

    size_t i = 0;
    while (fgets(line, sizeof(line), fp) && i < count) {
        // strtok modifies the string, splitting it by the comma delimiter
        char *token = strtok(line, ","); // Date (Skip)
        
        token = strtok(NULL, ","); 
        g_stocks[0].rows[i].open = atof(token); // Open
        
        token = strtok(NULL, ","); 
        g_stocks[0].rows[i].high = atof(token); // High
        
        token = strtok(NULL, ","); 
        g_stocks[0].rows[i].low = atof(token);  // Low
        
        token = strtok(NULL, ","); 
        g_stocks[0].rows[i].close = atof(token); // Close
        
        token = strtok(NULL, ","); 
        g_stocks[0].rows[i].volume = atof(token); // Volume
        
        g_stocks[0].rows[i].day = (int)i;
        i++;
    }

    fclose(fp);
    printf("Successfully loaded %zu rows of historical market data.\n", count);
}