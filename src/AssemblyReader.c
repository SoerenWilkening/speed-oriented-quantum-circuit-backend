//
// Created by Sören Wilkening on 20.11.24.
//

#include "../include/AssemblyReader.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024 // Maximum length of a line

typedef struct {
    char *addon;
    char *instruction;
    char *var1;
    char *var2;
    char *var3;
    int value;
} call;

call calls[1024];
int counter = 0;
int variable_counter = 0;

// Function to check if the string is a word (alphabetic characters only)
bool is_word(const char* str) {
    if (!str || *str == '\0') {
        return false; // Empty or null string is not a word
    }

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (!isalpha((unsigned char)str[i])) {
            return false; // Contains non-alphabetic characters
        }
    }

    return true;
}


char** extract_items_from_line(const char* line, size_t* item_count) {
    if (!line || !item_count) {
        return NULL;
    }

    *item_count = 0; // Initialize the item count

    size_t capacity = 10; // Initial capacity for the array of items
    char** items = malloc(capacity * sizeof(char*));

    // Create a modifiable copy of the input line
    char* line_copy = strdup(line);

    // Tokenize the line
    char* token = strtok(line_copy, " \t\n");
    while (token) {
        // Reallocate if the array is full
        if (*item_count >= capacity) {
            capacity *= 2;
            char** temp = realloc(items, capacity * sizeof(char*));
            items = temp;
        }

        // Duplicate the token and store it
        items[*item_count] = strdup(token);

        (*item_count)++;
        token = strtok(NULL, " \t\n"); // Get the next token
        if (token == NULL) break;
    }
    free(line_copy); // Free the duplicated line
    return items;
}

void word_to_call(char *word){
    if (!is_word(word)) return;
    if (strcmp(word, "INV") == 0) calls[counter].addon = word;
    else if (strcmp(word, "QUINT") == 0) calls[counter].addon = word;
    else if (strcmp(word, "QINT") == 0) calls[counter].addon = word;
    else if (strcmp(word, "UINT") == 0) calls[counter].addon = word;
    else if (strcmp(word, "INT") == 0) calls[counter].addon = word;
    else if (strcmp(word, "QBOOL") == 0) calls[counter].addon = word;
    else if (strcmp(word, "BOOL") == 0) calls[counter].addon = word;
    else if (strcmp(word, "BRANCH") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "IADD") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "PADD") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "ISUB") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "IMUL") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "IDIV") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "MOD") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "EQ") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "MEASURE") == 0) calls[counter].instruction = word;
    else if (strcmp(word, "IF") == 0) calls[counter].instruction = word;
    else{
        if (variable_counter == 0) calls[counter].var1 = word;
        if (variable_counter == 1) calls[counter].var2 = word;
        if (variable_counter == 2) calls[counter].var3 = word;
        variable_counter++;
    }
}

void value_to_call(char *word){
    if (is_word(word)) return;
    char *succ;
    calls[counter].value = strtol(word, &succ, 10);
}

void lines_to_call(char *line){
    if (line[0] == *"/" || line[1] == *"/") return;

    calls[counter].value = 0;
    calls[counter].addon = NULL;
    calls[counter].instruction = NULL;
    calls[counter].var1 = NULL;
    calls[counter].var2 = NULL;
    calls[counter].var3 = NULL;
    size_t count = 0;

    char **words = extract_items_from_line(line, &count);
    if (count == 0) return;
    for (int j = 0; j < count; ++j) {
        word_to_call(words[j]);
        value_to_call(words[j]);
    }
    printf("%s ", calls[counter].addon);
    printf("%s ", calls[counter].instruction);
    printf("%s ", calls[counter].var1);
    printf("%s ", calls[counter].var2);
    printf("%s ", calls[counter].var3);
    printf("%d ", calls[counter].value);
    counter++;
    printf("\n");
}


void ReadAssembly(char *asmb[], int num){
    for (int i = 0; i < num; ++i) {
        variable_counter = 0;
        // go through every line
        lines_to_call(asmb[i]);
    }
}

char *AsmbFromFile(){

    FILE *file = fopen("../assembly.pqsm", "r");
    int line_count = 0;

    size_t capacity = 10; // Initial capacity for the array of lines
    char** lines = malloc(capacity * sizeof(char*));

    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove the newline character if present
        buffer[strcspn(buffer, "\n")] = '\0';

        // Reallocate if the array is full
        if (line_count >= capacity) {
            capacity *= 2;
            char** temp = realloc(lines, capacity * sizeof(char*));
            lines = temp;
        }

        // Allocate memory for the line and copy the buffer
        lines[line_count] = strdup(buffer);

        line_count++;
    }

    ReadAssembly(lines, line_count);


    return NULL;
}
