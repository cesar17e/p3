#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    // For isatty(), read(), close()
#include <fcntl.h>     // For open()
#include <string.h>
#include <ctype.h>
#include "arraylist.h"
#include "builtInCommands.h" //Includes functions for built in commands
#define BUFLEN 1024
#define wordArraySize 500 // We can make this bigger 


/*
 * Updated command structure.
 * Instead of manually managing a char **args array,
 * we use an arraylist to store the command's arguments.
 */



typedef struct command {
    char *program;          // Program name (executable)
    arraylist_t *args;      // Arraylist of argument strings (for execv, use args->data)
    char *inputFile;        // Input redirection filename (if any)
    char *outputFile;       // Output redirection filename (if any)
    int pipePresent;        // Flag to indicate if a pipeline (|) exists
    struct command *next;   // Next command (for pipelines)
    enum { NONE, AND, OR } condition;  // Conditional operator relative to previous command
} command_t;

/*
 * Create a new command structure.
 * Allocates a new arraylist for arguments as well.
 */
command_t* createCommandStruct(void) {
    command_t *cmd = malloc(sizeof(command_t));
    if (cmd == NULL) {
        perror("malloc failed in createCommandStruct");
        return NULL;
    }
    cmd->program = NULL;  // Will be set later if needed.
    
    // Allocate and initialize the arraylist to store argument tokens.
    cmd->args = malloc(sizeof(arraylist_t));
    if (cmd->args == NULL) {
        perror("malloc failed for args arraylist");
        free(cmd);
        return NULL;
    }
    if (al_init(cmd->args, 10) != 0) {  // Start with capacity for 10 tokens.
        fprintf(stderr, "Error initializing args arraylist\n");
        free(cmd->args);
        free(cmd);
        return NULL;
    }
    cmd->inputFile = NULL;
    cmd->outputFile = NULL;
    cmd->pipePresent = 0;
    cmd->next = NULL;
    cmd->condition = NONE;
    return cmd;
}

/*
 * Free a command structure.
 * Frees all allocated strings stored in the args arraylist via al_destroy,
 * as well as any redirection strings and any piped commands.
 */
void freeCommandStruct(command_t *cmd) {
    if (cmd == NULL)
        return;
    
    if (cmd->program != NULL)
        free(cmd->program);
    
    if (cmd->args != NULL) {
        al_destroy(cmd->args);  // This frees the internal tokens and the data array.
        free(cmd->args);
    }
    
    if (cmd->inputFile != NULL)
        free(cmd->inputFile);
    if (cmd->outputFile != NULL)
        free(cmd->outputFile);
    
    if (cmd->next != NULL)
        freeCommandStruct(cmd->next);
    
    free(cmd);
}

/*
 * Helper function: Add a token to the command's arguments arraylist.
 * Duplicates the token and adds it using al_append.
 */
void addTokenToArgs(command_t *cmd, const char *token) {
    char *dup = strdup(token);
    if (!dup) {
        perror("strdup failed in addTokenToArgs");
        exit(EXIT_FAILURE);
    }
    if (al_append(cmd->args, dup) != 0) {
        fprintf(stderr, "Failed to add token to args arraylist\n");
        free(dup);
        exit(EXIT_FAILURE);
    }
}

/*
 * finalizeArgs: Append a NULL pointer to the end of our args arraylist.
 * This ensures that cmd->args->data (a char ** pointer) is properly NULL-terminated.
 */
void finalizeArgs(command_t *cmd) {
    if (al_append(cmd->args, NULL) != 0) {
        fprintf(stderr, "Failed to finalize args arraylist\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Process the tokens in the provided arraylist 'list' (each token is a null-terminated string)
 * and build a command structure using the arraylist for the arguments.
 */
void processCommand(arraylist_t *list) {
    if (list->length == 0) {
        return; // Nothing to process.
    }

    // Create the head of the command chain.
    command_t *commandHead = createCommandStruct();
    if (!commandHead) {
        fprintf(stderr, "Failed to create command structure\n");
        return;
    }

    command_t *cmd = commandHead;  // Pointer to current command.

    // Iterate through tokens from the token list.
    for (int i = 0; i < list->length; i++) {
        char *token = list->data[i];
        
        // Check for input redirection token "<".
        if (strcmp(token, "<") == 0) {
            i++;  // Next token should be the input file name.
            if (i < list->length) {
                cmd->inputFile = strdup(list->data[i]);
                if (!cmd->inputFile) {
                    perror("strdup failed for inputFile");
                    freeCommandStruct(commandHead);
                    return;
                }
            } else {
                fprintf(stderr, "Syntax error: missing input file after '<'\n");
                freeCommandStruct(commandHead);
                return;
            }
        }
        // Check for output redirection token ">".
        else if (strcmp(token, ">") == 0) {
            i++;  // Next token should be the output file name.
            if (i < list->length) {
                cmd->outputFile = strdup(list->data[i]);
                if (!cmd->outputFile) {
                    perror("strdup failed for outputFile");
                    freeCommandStruct(commandHead);
                    return;
                }
            } else {
                fprintf(stderr, "Syntax error: missing output file after '>'\n");
                freeCommandStruct(commandHead);
                return;
            }
        }
        // Check for pipeline operator "|".
        else if (strcmp(token, "|") == 0) {
            cmd->pipePresent = 1;
            cmd->next = createCommandStruct();
            if (!cmd->next) {
                fprintf(stderr, "Failed to create command struct for pipeline\n");
                freeCommandStruct(commandHead);
                return;
            }
            cmd = cmd->next;  // Switch to the next command structure.
        }
        // Check for conditional tokens "and" / "or".
        else if (strcmp(token, "and") == 0 || strcmp(token, "or") == 0) {
            if (strcmp(token, "and") == 0) {
                cmd->condition = AND;
            } else {
                cmd->condition = OR;
            }         
        }
        // Otherwise, treat token as a normal argument.
        else {
            addTokenToArgs(cmd, token);
        }
    }
    
    // Finalize the args arraylist to ensure NULL termination.
    finalizeArgs(commandHead);

    // At this point, the command structure is ready for execution.
    // For demonstration purposes, we print out the arguments:
    if (commandHead->args && commandHead->args->data) {
        printf("Command arguments:\n");
        for (int i = 0; commandHead->args->data[i] != NULL; i++) {
            printf("  %s\n", commandHead->args->data[i]);
        }
    }
    
    // Now here we want to execute the command
    // executeCommand(commandHead);
    
    // Finally, free the command structure (including any piped commands) after execution.
    freeCommandStruct(commandHead);
}


/*
it splits the given command into tokens using whitespace as the delimiter.
Also, it stops processing further characters if a # is encountered.
NOTE: For simplicity, you are permitted to assume that >, <, and | are separated from other tokens by whitespace.
*/

void tokenizeCommand(const char *command, arraylist_t *list, int linelen) {
    int insideAWord = 0;         // 0 = not inside a token, 1 = inside a token.
    char wordArray[wordArraySize];
    int wordIndex = 0;       
    
    // Clear any previous tokens that was made by a previous command
    al_clear(list);
    
    for (int i = 0; i < linelen; i++) {
        char c = command[i];
        
        // If # is encountered, stop processing the rest as it signals a comment.
        if (c == '#') {
            break;
        }
        
        if (isspace(c)) {
            // End of token if we're currently in one.
            if (insideAWord) {
                wordArray[wordIndex] = '\0'; // Null-terminate the token.
                int token_len = wordIndex;  
                char *dup = malloc(token_len + 1);  // +1 for the null terminator.
                if (!dup) {
                    perror("malloc error on duplicate string in tokenize_command");
                    return;
                }
                strcpy(dup, wordArray);
                // Append the token to the array list
                if (al_append(list, dup) != 0) {
                    fprintf(stderr, "Failed to add token to the array list\n");
                    free(dup);
                    return;
                }
        
                // Reset for the next word
                wordIndex = 0;
                insideAWord = 0;
            }
        } else {
            // Add the character to the buffer
            if (wordIndex < wordArraySize - 1) {
                wordArray[wordIndex++] = c;
            }
            insideAWord = 1;
        }
    }
    
    // If a token was being built at the end of the command we must finish it
    if (insideAWord) {
        wordArray[wordIndex] = '\0';
        int token_len = wordIndex;
        char *dup = malloc(token_len + 1);
        if (!dup) {
            perror("malloc");
            return;
        }
        strcpy(dup, wordArray);
        
        if (al_append(list, dup) != 0) {
            fprintf(stderr, "Failed to add token to the array list\n");
            free(dup);
            return;
        }
    }
}


/*
So this reads the lines either from batch mode or interactive moden which we then send to tokenize command to seperate the words in an array list, after that we send it to a processing command where the actual program begins
*/
void process_lines(int fd, arraylist_t *list, int interactive) {
    int n = 0;
    char buf[BUFLEN];
    int pos;
    char *line = NULL;
    int linelen = 0;
    int bytes;
    int segstart, seglen;

    while ((bytes = read(fd, buf, BUFLEN)) > 0) {
     
        segstart = 0;
        for (pos = 0; pos < bytes; pos++) {
            if (buf[pos] == '\n') {
                seglen = pos - segstart;
                line = realloc(line, linelen + seglen + 1);
                if (!line) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
                memcpy(line + linelen, buf + segstart, seglen);
                linelen += seglen;
                line[linelen] = '\0';
                
                // At this point the line is complete and it holds a complete command.
                // Tokenize it
                tokenizeCommand(line, list, linelen);

                processCommand(list);

                // In interactive mode, print the prompt for the next command.
                if (interactive) {
                    printf("mysh> ");
                    fflush(stdout);
                }
                      
                // Clean up for the next command.
                free(line);
                line = NULL;
                linelen = 0;
                n++;
                segstart = pos + 1;
            }
        }
        if (segstart < pos) {
            seglen = pos - segstart;
            line = realloc(line, linelen + seglen);
            if (!line) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            memcpy(line + linelen, buf + segstart, seglen);
            linelen += seglen;
        }
    }
    // Process any leftover command without a newline.
    if (line && linelen > 0) {
        line = realloc(line, linelen + 1);
        if (!line) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        line[linelen] = '\0';
        tokenizeCommand(line, list, linelen);
        printf("Echoed command: ");
        processCommand(list);
        free(line);
    }
}

// Main--> we set up input, determine interactive mode, and process commands.
int main(int argc, char *argv[]) {
    int fd;
    if (argc > 1) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }
    } else {
        fd = STDIN_FILENO;
    }
    
    int interactive = isatty(fd);
    if (interactive) {
        printf("Welcome to my shell!\n");
    }
    
    // Allocate and initialize the array list.
    arraylist_t *list = malloc(sizeof(arraylist_t));
    if (!list) {
        perror("malloc");
        perror("Problem with arraylist allocation");
    }
    if (al_init(list, 40) != 0) {
        fprintf(stderr, "Error initializing array list\n");
        perror("Problem with arraylist declaration");
    }
    
    // In interactive mode, print a prompt before processing commands.
    if (interactive) {
        printf("mysh> ");
        fflush(stdout);
    }
    
    process_lines(fd, list, interactive);
    
    if (interactive) {
        printf("Exiting my shell.\n");
    }
    if (fd != STDIN_FILENO) {
        close(fd);
    }
    
    al_destroy(list);
    free(list);
    
    return 0;
}
