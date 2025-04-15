#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    // For isatty(), read(), close()
#include <fcntl.h>     // For open()
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include "arraylist.h"
#include "builtInCommands.h" //Includes functions for built in commands
#define BUFLEN 1024
#define wordArraySize 500 // We can make this bigger 
int prevExitStatus = 0;  // Assume success (0) by default

/*
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


// is_builtin_command: returns 1 if cmd is one of the built-in commands.
int is_builtin_command(const char *cmd) {
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "exit") == 0 || strcmp(cmd, "die") == 0 ||
            strcmp(cmd, "which") == 0);
}

// handle_builtin_command: dispatches to the appropriate built-in handler.
// It uses cmd->program if set; otherwise, it takes the first token in cmd->args->data.
void handle_builtin_command(command_t *cmd) {
    const char *cmdName = (cmd->program != NULL) ? cmd->program : cmd->args->data[0];

    if (strcmp(cmdName, "cd") == 0) {
        builtin_cd(cmd->args);
    } else if (strcmp(cmdName, "pwd") == 0) {
        builtin_pwd(cmd->args);
    } else if (strcmp(cmdName, "exit") == 0) {
        builtin_exit(cmd->args);
    } else if (strcmp(cmdName, "die") == 0) {
        builtin_die(cmd->args);
    } else if (strcmp(cmdName, "which") == 0) {
        builtin_which(cmd->args);
    } else {
        fprintf(stderr, "Unknown built-in command: %s\n", cmdName);
    }
}


/*
 * simulateExecuteCommand: Simulates executing a command (or chain of piped commands)
 * with built-in handling of conditionals.
 *
 * If a command is conditional ("and"/"or"), then based on the previous exit status,
 * the command (or entire pipeline) might be skipped.
 */
// void simulateExecuteCommand(command_t *cmd) {
//     // Check conditionals in the current command:
//     if (cmd->condition != NONE) {
//         if (cmd->condition == AND && prevExitStatus != 0) {
//             // 'and' condition not met (previous command failed)
//             printf("Skipping command due to 'and' condition (prevExitStatus = %d).\n", prevExitStatus);
//             return;
//         }
//         if (cmd->condition == OR && prevExitStatus == 0) {
//             // 'or' condition not met (previous command succeeded)
//             printf("Skipping command due to 'or' condition (prevExitStatus = %d).\n", prevExitStatus);
//             return;
//         }
//     }

//     // If the command is part of a pipeline, simulate the entire pipeline:
//     if (cmd->pipePresent) {
//         printf("Simulating execution of pipeline: ");
//         command_t *cur = cmd;
//         while (cur != NULL) {
//             const char *cmdName = cur->program ? cur->program : cur->args->data[0];
//             printf("'%s' ", cmdName);
//             cur = cur->next;
//         }
//         printf("\n");
//         // For simulation purposes, assume the entire pipeline succeeds.
//         prevExitStatus = 0;
//     } else {
//         // Single command simulation:
//         const char *cmdName = cmd->program ? cmd->program : cmd->args->data[0];
//         printf("Simulating execution of command: %s\n", cmdName);

//         // Check if it's a built-in command.
//         if (is_builtin_command(cmd->program ? cmd->program : cmd->args->data[0])) {
//             printf("Executing built-in command '%s'.\n", cmdName);
//             // Here, you can call your built-in handler (or simulate its behavior).
//             // For example:
//             handle_builtin_command(cmd);
//         } else {
//             printf("Would execute external command '%s'.\n", cmdName);
//         }
//         // For simulation purposes, assume the command succeeds.
//         prevExitStatus = 0;
//     }
// }

/*
 * executeCommand:
 * Executes a single command, or a simple pipeline of two commands, according to:
 * - Conditional operators: if the command starts with "and" or "or", we decide whether to
 *   execute it based on prevExitStatus.
 * - Pipelines: if cmd->pipePresent is set, we assume a two-command pipeline.
 * - Redirection: input (<) and output (>) redirection are performed in the appropriate child
 *   processes (or, for built-in commands that modify shell state, in the parent after temporarily
 *   modifying the file descriptors).
 *
 * For built-in commands (cd, pwd, exit, die, which) that run in the main shell process,
 * we handle them directly when no pipeline is involved. However, if they appear in a pipeline,
 * they must be forked (and their effect on the shell process will not persist).
 */
void executeCommand(command_t *cmd) {
    // ======================================================
    // 1. Handle Conditional Execution:
    // ======================================================
    if (cmd->condition != NONE) {
        if (cmd->condition == AND && prevExitStatus != 0) {
            // Skip execution if previous command failed.
            fprintf(stdout, "Skipping command due to 'and' condition (prevExitStatus = %d).\n", prevExitStatus);
            return;
        }
        if (cmd->condition == OR && prevExitStatus == 0) {
            // Skip execution if previous command succeeded.
            fprintf(stdout, "Skipping command due to 'or' condition (prevExitStatus = %d).\n", prevExitStatus);
            return;
        }
    }

    // ======================================================
    // 2. Pipeline Execution (assumed to be two commands)
    // ======================================================
    if (cmd->pipePresent && cmd->next) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            prevExitStatus = 1;
            return;
        }
        
        // Fork first child for the left command.
        pid_t pid1 = fork();
        if (pid1 < 0) {
            perror("fork");
            prevExitStatus = 1;
            return;
        }
        if (pid1 == 0) {
            // Child 1: Execute left-side command.
            // Close unused read end.
            close(pipefd[0]);
            // Redirect standard output to pipe's write end.
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2 (child1)");
                exit(1);
            }
            close(pipefd[1]);
            
            // For simplicity, we assume pipeline subcommands have no redirection.
            const char *cmdName = (cmd->program != NULL) ? cmd->program : cmd->args->data[0];
            if (is_builtin_command(cmdName)) {
                // In a pipeline, run built-in in the child.
                handle_builtin_command(cmd);
                exit(0);
            } else {
                // If cmdName is a bare name (no '/') search in the prescribed directories.
                char executablePath[4096];
                if (strchr(cmdName, '/')) {
                    strncpy(executablePath, cmdName, sizeof(executablePath));
                    executablePath[sizeof(executablePath)-1] = '\0';
                } else {
                    const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
                    int found = 0;
                    for (int i = 0; i < 3; i++) {
                        snprintf(executablePath, sizeof(executablePath), "%s/%s", dirs[i], cmdName);
                        if (access(executablePath, X_OK) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "%s: command not found\n", cmdName);
                        exit(1);
                    }
                }
                execv(executablePath, cmd->args->data);
                perror("execv");
                exit(1);
            }
        }

        // Fork second child for the right-side command (cmd->next).
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork");
            prevExitStatus = 1;
            return;
        }
        if (pid2 == 0) {
            // Child 2: Execute right-side command.
            close(pipefd[1]); // Close write end.
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2 (child2)");
                exit(1);
            }
            close(pipefd[0]);
            
            const char *cmdName = (cmd->next->program != NULL) ? cmd->next->program : cmd->next->args->data[0];
            if (is_builtin_command(cmdName)) {
                handle_builtin_command(cmd->next);
                exit(0);
            } else {
                char executablePath[4096];
                if (strchr(cmdName, '/')) {
                    strncpy(executablePath, cmdName, sizeof(executablePath));
                    executablePath[sizeof(executablePath)-1] = '\0';
                } else {
                    const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
                    int found = 0;
                    for (int i = 0; i < 3; i++) {
                        snprintf(executablePath, sizeof(executablePath), "%s/%s", dirs[i], cmdName);
                        if (access(executablePath, X_OK) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "%s: command not found\n", cmdName);
                        exit(1);
                    }
                }
                execv(executablePath, cmd->next->args->data);
                perror("execv");
                exit(1);
            }
        }
        
        // Parent: close pipe file descriptors.
        close(pipefd[0]);
        close(pipefd[1]);
        
        // Wait for both children.
        int status;
        waitpid(pid1, NULL, 0);
        waitpid(pid2, &status, 0);
        if (WIFEXITED(status))
            prevExitStatus = WEXITSTATUS(status);
        else
            prevExitStatus = 1;
        
        return;
    }  // End pipeline block.

    // ======================================================
    // 3. Single Command Execution (no pipeline)
    // ======================================================
    const char *cmdName = (cmd->program != NULL) ? cmd->program : cmd->args->data[0];
    int isBuiltin = is_builtin_command(cmdName);

    // --------------------
    // 3a. For built-in commands executed alone.
    // --------------------
    if (isBuiltin) {
        // For built-ins that affect shell state, run them in the parent's process.
        // If redirection is specified, temporarily redirect standard file descriptors.
        int saved_stdin = -1, saved_stdout = -1;
        int fdIn = -1, fdOut = -1;
        
        if (cmd->inputFile) {
            saved_stdin = dup(STDIN_FILENO);
            fdIn = open(cmd->inputFile, O_RDONLY);
            if (fdIn < 0) {
                perror("open input");
                prevExitStatus = 1;
                return;
            }
            if (dup2(fdIn, STDIN_FILENO) < 0) {
                perror("dup2 input");
                close(fdIn);
                prevExitStatus = 1;
                return;
            }
            close(fdIn);
        }
        if (cmd->outputFile) {
            saved_stdout = dup(STDOUT_FILENO);
            fdOut = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fdOut < 0) {
                perror("open output");
                prevExitStatus = 1;
                if (saved_stdin != -1) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return;
            }
            if (dup2(fdOut, STDOUT_FILENO) < 0) {
                perror("dup2 output");
                close(fdOut);
                prevExitStatus = 1;
                if (saved_stdin != -1) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return;
            }
            close(fdOut);
        }
        
        // Execute the built-in command in the parent's process.
        handle_builtin_command(cmd);
        prevExitStatus = 0;  // Assume success; you may adjust based on error checks.
        
        // Restore original file descriptors.
        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        return;
    }
    
    // --------------------
    // 3b. For external commands.
    // --------------------
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        prevExitStatus = 1;
        return;
    }
    if (pid == 0) { 
        // Child process: handle redirection then exec.
        if (cmd->inputFile) {
            int fdIn = open(cmd->inputFile, O_RDONLY);
            if (fdIn < 0) {
                perror("open input");
                exit(1);
            }
            if (dup2(fdIn, STDIN_FILENO) < 0) {
                perror("dup2 input");
                close(fdIn);
                exit(1);
            }
            close(fdIn);
        }
        if (cmd->outputFile) {
            int fdOut = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fdOut < 0) {
                perror("open output");
                exit(1);
            }
            if (dup2(fdOut, STDOUT_FILENO) < 0) {
                perror("dup2 output");
                close(fdOut);
                exit(1);
            }
            close(fdOut);
        }
        
        // Determine the correct executable path.
        char executablePath[4096];
        if (strchr(cmdName, '/')) {
            strncpy(executablePath, cmdName, sizeof(executablePath));
            executablePath[sizeof(executablePath)-1] = '\0';
        } else {
            const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
            int found = 0;
            for (int i = 0; i < 3; i++) {
                snprintf(executablePath, sizeof(executablePath), "%s/%s", dirs[i], cmdName);
                if (access(executablePath, X_OK) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "%s: command not found\n", cmdName);
                exit(1);
            }
        }
        
        // Execute the external command.
        execv(executablePath, cmd->args->data);
        perror("execv");
        exit(1);
    } else {
        // Parent: wait for the child to finish.
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            prevExitStatus = WEXITSTATUS(status);
        else
            prevExitStatus = 1;
        return;
    }
}


/*
 * Process the tokens in the provided arraylist 'list' (each token is a null-terminated string)
 * and build a command structure using the arraylist for the arguments.
 */
/*
 * Process the tokens in the provided arraylist 'list'
 * and build a command structure using the arraylist for the arguments.
 * Then, simulate executing the command.
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
            cmd->condition = (strcmp(token, "and") == 0) ? AND : OR;
            // Do not add conditional tokens to the arguments list.
        }
        // Otherwise, treat token as a normal argument.
        else {
            addTokenToArgs(cmd, token);
        }
    }
    
    // Finalize the args arraylist to ensure NULL termination.
    finalizeArgs(commandHead);

    // --- Instead of executing, simulate the execution of the command ---
    executeCommand(commandHead);

    // Finally, free the command structure (including any piped commands).
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
