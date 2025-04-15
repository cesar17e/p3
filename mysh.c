#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <fcntl.h>    
#include <string.h>
#include <ctype.h>
#include <sys/wait.h> 
#include <dirent.h> 
#include "arraylist.h" //Includes the arraylist functions we need
#include "builtInCommands.h" //Includes functions for built in commands

#define BUFLEN 1024 // Standard buffer length we can make this bigger
#define wordArraySize 500 // The word array size for the tokenizer command
int prevExitStatus = 0;  // Assume success is 0 by default
int firstTimeRunning = 0; 

/*
 * The struct command where it will hold all the needed data when we process this within the function processCommand
 */

typedef struct command {
    char *program;          // The name of the program, which is really the executable
    arraylist_t *args;      // Arraylist of argument strings, for execv use args->data as it holds the string names
    char *inputFile;        // Input redirection filename 
    char *outputFile;       // Output redirection filename 
    int pipePresent;        // Flag that shows if a pipe exists
    struct command *next;   // When pipelines exist we need to seperate commands so we will use a linked list of commands
    enum { NONE, AND, OR } condition;  // Conditional operator relative to previous command
}command_t;

/*
 * This function creates a new commandStructure
 * Allocates a new arraylist for arguments as well
 */
command_t *createCommandStruct() {
    command_t *cmd = malloc(sizeof(command_t));
    if (cmd == NULL) {
        perror("malloc failed in createCommandStruct");
        return NULL;
    }
    cmd->program = NULL; 
    // Allocate and initialize the arraylist to store argument tokens

    cmd->args = malloc(sizeof(arraylist_t));
    if (cmd->args == NULL) {
        perror("malloc failed for args arraylist");
        free(cmd);
        return NULL;
    }
    if (al_init(cmd->args, 10) != 0) {  // Start with capacity for 10 words
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
 * Free a command structure
 * Frees all allocated strings stored in the args arraylist with the destroy function, as well as any redirection strings and any piped commands
 */

void freeCommandStruct(command_t *cmd) {
    if (cmd == NULL){
        return;
    }
    if (cmd->program != NULL){
        free(cmd->program);
    }
    if (cmd->args != NULL){
        al_destroy(cmd->args);  
        free(cmd->args);
    }
    if (cmd->inputFile != NULL){
        free(cmd->inputFile);
    }
    if (cmd->outputFile != NULL){
        free(cmd->outputFile);
    } 
    if (cmd->next != NULL){
        freeCommandStruct(cmd->next);
    }
    
    free(cmd);
}

/*
 * This function will add a token to the struct commands arguments arraylist
 * It will duplicate the token and adds it using al_append
 */

void addTokenToArgs(command_t *cmd, const char *token) {
    char *dup = strdup(token); //We will use stdup alot it just duplicates the string instead of us using malloc alot
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
 * finalizeArgs--> it adds a null pointer to the end of our args arraylist
 * This mkaes sure that cmd->args->data is properly null-terminated so we dont get any errors
 */

void finalizeArgs(command_t *cmd) {
    if (al_append(cmd->args, NULL) != 0) {
        fprintf(stderr, "Failed to finalize args arraylist\n");
        exit(EXIT_FAILURE);
    }
}

// Helper function to know if its built in or not
int isBuiltInCommand(const char *cmd) {
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "exit") == 0 || strcmp(cmd, "die") == 0 ||
            strcmp(cmd, "which") == 0);
}

// This will handle our built in commands, it will send to the built-in function we made
// It uses cmd->program if it is set, otherwise it takes the first token in cmd->args->data.
void handleBuiltInCommands(command_t *cmd) {
    const char *cmdName;

    if (cmd->program != NULL) {
        cmdName = cmd->program;
    } else {
        cmdName = cmd->args->data[0];
    }

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
  executeCommand, the main functions, alot of test cases
  Executes a single command, or a pipeline of two commands
  - Conditional operators: if the command starts with and or "or" we decide whether to execute it based on firstTimeRunning global
  -Pipelines: if cmd->pipePresent is set, we assume a two-command pipeline.
  -Redirection: input and output redirection are done in the child processes 
  -Built ins can be run with additional args we will handle them directly when no pipeline is involved. If they appear in a pipeline, we will fork them.
 */
void executeCommand(command_t *cmd) {
    //Conditional Execution--> can not run when it is the very first command
    if (cmd->condition != NONE) {
        //!This is the case where it is the very command
        if(firstTimeRunning == 0){
            fprintf(stdout, "Error: 'and' or 'or' command provided when this is the first command run. \n");
            return;
        }
        if (cmd->condition == AND && prevExitStatus != 0) {
            fprintf(stdout, "Skipping command due to 'and' condition (prevExitStatus = %d).\n", prevExitStatus);
            return;
        }
        if (cmd->condition == OR && prevExitStatus == 0) {
            fprintf(stdout, "Skipping command due to 'or' condition (prevExitStatus = %d).\n", prevExitStatus);
            return;
        }
    }
    firstTimeRunning = 1;

    //Pipeline Execution
    //Logic we need a pipe where one child writes data and the other reads it, use pipe and and dup to change the fd's
    if (cmd->pipePresent && cmd->next) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            prevExitStatus = 1;
            return;
        }
        
        // Fork first child for the left command.
        pid_t pid1 = fork();
        if (pid1 < 0) { //Check for errors
            perror("fork");
            prevExitStatus = 1;
            return;
        }
        if (pid1 == 0) {
            //We are now in child 1 we will execute the left side command
            close(pipefd[0]);
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) { //Check for errors
                perror("dup2 (child1)");
                exit(1);
            }
            close(pipefd[1]);

            //Based on the project we can assume pipeline subcommands have no redirection
            const char *cmdName;
            if (cmd->program != NULL) {
                cmdName = cmd->program;
            } else {
                cmdName = cmd->args->data[0];
            }
            if (isBuiltInCommand(cmdName)) {
                handleBuiltInCommands(cmd);
                exit(0);
            } else {
                //If cmdName has no "/" we search in these directories
                char executablePath[4096];
                if (strchr(cmdName, '/')) { // strchr finds the first instance of this char within a string
                    strncpy(executablePath, cmdName, sizeof(executablePath));
                    executablePath[sizeof(executablePath)-1] = '\0';
                } else {
                    const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
                    int found = 0;
                    for (int i = 0; i < 3; i++) {
                        strncpy(executablePath, dirs[i], sizeof(executablePath) - 1);
                        executablePath[sizeof(executablePath) - 1] = '\0';  //Make sure the null is there
                        strncat(executablePath, "/", sizeof(executablePath) - strlen(executablePath) - 1);
                        strncat(executablePath, cmdName, sizeof(executablePath) - strlen(executablePath) - 1);
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
                execv(executablePath, cmd->args->data);  //Finally exec takes over
                perror("execv failed in 1st child pipe process");
                exit(1);
            }
        }

        //Now we fork second child for the right side command
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork");
            prevExitStatus = 1;
            return;
        }
        if (pid2 == 0) {
            close(pipefd[1]); // Close write end.
            if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                perror("dup2 (child2)");
                exit(1);
            }
            close(pipefd[0]);
            
            const char *cmdName;
            if (cmd->next->program != NULL) {
                cmdName = cmd->next->program;
            } else {
                cmdName = cmd->next->args->data[0];
            }
            if (isBuiltInCommand(cmdName)) {
                handleBuiltInCommands(cmd->next);
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
                        //Re-use our old code from before
                        strncpy(executablePath, dirs[i], sizeof(executablePath) - 1);
                        executablePath[sizeof(executablePath) - 1] = '\0'; 
                        strncat(executablePath, "/", sizeof(executablePath) - strlen(executablePath) - 1);
                        strncat(executablePath, cmdName, sizeof(executablePath) - strlen(executablePath) - 1);
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
                execv(executablePath, cmd->next->args->data); //Finally exec takes over
                perror("execv failed in second child pipe process"); //If it fails check
                exit(1);
            }
        }
        
        // We are now done and were in the parent close pipe file descriptors
        close(pipefd[0]);
        close(pipefd[1]);
        
        // Wait for both children
        int status;
        waitpid(pid1, NULL, 0);
        waitpid(pid2, &status, 0);
        if (WIFEXITED(status)){
            prevExitStatus = WEXITSTATUS(status);
        }
        else{
            prevExitStatus = 1;
        }
        
        return;
    }  //----End of pipeline block---

    //Single Command Execution, no pipeline exists

    const char *cmdName;
    if (cmd->program != NULL) {
        cmdName = cmd->program;
    } else {
        cmdName = cmd->args->data[0];
    }
    int isBuiltin = isBuiltInCommand(cmdName);

    //--->For built-in commands executed alone
    if (isBuiltin) {
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
        
        // Execute the builtIn command in the parent
        handleBuiltInCommands(cmd);
        prevExitStatus = 0; 
        
        //Restore original file descriptor because we need it later
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
    
    // Now for exec commands.
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        prevExitStatus = 1;
        return;
    }
    if (pid == 0) { 
        // Child process--> handle redirection then exec.
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
        
        // We now must get the correct executable path
        char executablePath[4096];
        if (strchr(cmdName, '/')) {
            strncpy(executablePath, cmdName, sizeof(executablePath));
            executablePath[sizeof(executablePath)-1] = '\0';
        } else {
            const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
            int found = 0;
            for (int i = 0; i < 3; i++) {
                strncpy(executablePath, dirs[i], sizeof(executablePath) - 1);
                executablePath[sizeof(executablePath) - 1] = '\0';  // Ensure null-termination.
                strncat(executablePath, "/", sizeof(executablePath) - strlen(executablePath) - 1);
                strncat(executablePath, cmdName, sizeof(executablePath) - strlen(executablePath) - 1);
            
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
        
        // Call exec and then run it given the path we created
        execv(executablePath, cmd->args->data);
        perror("execv");
        exit(1);
    } else {
        // Now in parent we must wait for the child to finish.
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            prevExitStatus = WEXITSTATUS(status);
        else
            prevExitStatus = 1;
        return;
    }
}


// ExpandWildcard: If token contains '*' we have to expand it by matching files in a directory
// If token contains a '/', use the part before the last '/' as the directory, otherwise, search in the current directory
void expandWildcard(command_t *cmd, const char *token) {
    const char *slash = strrchr(token, '/');
    char dirname[1024];
    char pattern[1024];

    if (slash) {
        int dirlen = slash - token;
        strncpy(dirname, token, dirlen);
        dirname[dirlen] = '\0';
        strcpy(pattern, slash + 1);
    } else {
        strcpy(dirname, ".");
        strcpy(pattern, token);
    }

    // Find the * in the pattern.
    char *asterisk = strchr(pattern, '*');
    if (!asterisk) {
        addTokenToArgs(cmd, token);
        return;
    }

    // Split the pattern
    *asterisk = '\0';  
    char *before = pattern;         // Everything before * 
    char *after = asterisk + 1;      // Everything after * 
    //It might be empty too idk check later

    DIR *dir = opendir(dirname);
    if (!dir) {
        // If the directory cannot be opened, fall back to adding the original token.
        addTokenToArgs(cmd, token);
        return;
    }

    struct dirent *entry;
    int matches = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Optionally skip hidden files unless the pattern begins with '.'
        if (before[0] != '.' && entry->d_name[0] == '.')
            continue;

        int len = strlen(entry->d_name);
        int plen = strlen(before);
        int slen = strlen(after);

        // The filename must be at least as long as the combined before and affter lines
        if (len < plen + slen){
            continue;
        }

        if (strncmp(entry->d_name, before, plen) != 0){
            continue;
        }

        if (slen > 0) {
            if (strcmp(entry->d_name + len - slen, after) != 0){
                continue;
            }
        }

        // Only add the entry if it strictly matches the pattern
        char fullpath[2048];
        if (strcmp(dirname, ".") == 0) {
            strncpy(fullpath, entry->d_name, sizeof(fullpath) - 1);
            fullpath[sizeof(fullpath) - 1] = '\0';
        } else {
            strncpy(fullpath, dirname, sizeof(fullpath) - 1);
            fullpath[sizeof(fullpath) - 1] = '\0';
            strncat(fullpath, "/", sizeof(fullpath) - strlen(fullpath) - 1);
            strncat(fullpath, entry->d_name, sizeof(fullpath) - strlen(fullpath) - 1);
        }
        addTokenToArgs(cmd, fullpath);
        matches++;
        
    }

    closedir(dir);

    // If no entries matched we just add the original word like usual
    if (matches == 0){
        addTokenToArgs(cmd, token);
    }
}


/* Here we will process the commands given the provided arraylist from tokenizer
  and build a command structure using the arraylist for the arguments, then we will send to execute
 */
void processCommand(arraylist_t *list) {
    if (list->length == 0) {
        return; // Nothing to process.
    }
    
    // Dont allow a conditional operator as the first token when no command has run yet
    if (firstTimeRunning == 0 &&
       (strcmp(list->data[0], "and") == 0 || strcmp(list->data[0], "or") == 0)) {
        fprintf(stderr, "Eorro: 'and' or 'or' command provided when this is the first command run\n");
        return;
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
        
        if (strcmp(token, "<") == 0) { //If token == <
            i++;  // Go up one we add the file name
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
        else if (strcmp(token, ">") == 0) { //If token == >
            i++;  //Go up one we add the file name
            if (i < list->length) {
                cmd->outputFile = strdup(list->data[i]);
                if (!cmd->outputFile) {
                    perror("strdup failed for outputFile");
                    freeCommandStruct(commandHead);
                    return;
                }
            } else {
                fprintf(stderr, "Error: missing output file after '>'\n");
                freeCommandStruct(commandHead);
                return;
            }
        }
        else if (strcmp(token, "|") == 0) { //If token == |
            cmd->pipePresent = 1;
            cmd->next = createCommandStruct();
            if (!cmd->next) { //Something went wrong
                fprintf(stderr, "Pipeline has no next command failed to proceed\n");
                freeCommandStruct(commandHead);
                return;
            }
            cmd = cmd->next;  // Switch to the next command structure.
        }
        else if (strcmp(token, "and") == 0 || strcmp(token, "or") == 0) { //If token == and or or
            // If encountered after the first command in the chain, we have an error this cant happen
            if (cmd != commandHead) {
                fprintf(stderr, "Error: conditional operator cannot appear after a pipe\n");
                freeCommandStruct(commandHead);
                return;
            }
            cmd->condition = (strcmp(token, "and") == 0) ? AND : OR;
            // Do not add the conditional token to the arguments list.
        }
        else { //No special tokens besides *
            if (strchr(token, '*') != NULL) {
                expandWildcard(cmd, token);
            } else {
                addTokenToArgs(cmd, token);
            }
        }
    }
    
    //Finally we send it to here for null term
    finalizeArgs(commandHead);
    
    if (commandHead->args->data[0] == NULL) {
        fprintf(stderr, "Error: missing command after conditional operator.\n");
        freeCommandStruct(commandHead);
        return;
    } //Nothing to execute 


    //If program name isnt made just assume its as index 0
    if (commandHead->program == NULL && commandHead->args->data[0] != NULL) {
        commandHead->program = strdup(commandHead->args->data[0]);
        if (!commandHead->program) {
            perror("strdup failed for program name");
            freeCommandStruct(commandHead);
            return;
        }
    }

    command_t *temp = commandHead->next;
    while (temp != NULL) {
        if (temp->condition != NONE) {
            fprintf(stderr, "Error: conditional operator cannot appear after a pipe\n");
            freeCommandStruct(commandHead);
            return;
        }
        temp = temp->next;
    }
    
    //Now we enter the main stuff, still gotta fix it later
    executeCommand(commandHead);
    
    firstTimeRunning = 1; //Mark that we have now run at least one command for the conditional logic stuff

    freeCommandStruct(commandHead); //Free it at the end
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
        
        if (al_append(list, dup) != 0){
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
        printf("Good bye! Exiting my shell\n");
    }
    if (fd != STDIN_FILENO) {
        close(fd);
    }
    
    al_destroy(list);
    free(list);
    
    return 0;
}