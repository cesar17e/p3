#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // For chdir(), getcwd(), access()
#include <string.h> 
#include "builtInCommands.h"

/*
 * The cd function, we used chdir to go into the directory
 * Notes: Since finalizeArgs appends the NULL, the actual number of tokens is (list->length - 1).
 */
void builtin_cd(arraylist_t *list) {
    int argCount = list->length - 1; //Not including the null char
    if (argCount != 2) { //Cd must expect one arg
        fprintf(stderr, "cd: expected one argument\n");
        return;
    }
    if (chdir(list->data[1]) != 0) {
        perror("cd");
    }
}

/*
 * pwd - it expects only the command "pwd" (no extra arguments).
 */
void builtin_pwd(arraylist_t *list) {
    (void)list;  // pwd doesnt need extra arguments.
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return;
    }
    printf("%s\n", cwd);
    fflush(stdout);  //Using flush instead of write
}

/*
 * builtin_exit--> it expects only the command "exit" (no extra arguments).
 */
void builtin_exit(arraylist_t *list) {
    int argCount = list->length - 1;
    if (argCount != 1) {
        fprintf(stderr, "exit: expected no arguments\n");
        return;
    }
    exit(EXIT_SUCCESS);
}

/*
 * builtin_die:
 * Prints error messages (if any) following the "die" command, then exits with failure.
 */
void builtin_die(arraylist_t *list) {
    int argCount = list->length - 1;
    if (argCount < 1) {  // Should at least have "die"
        fprintf(stderr, "die: missing message\n");
        exit(EXIT_FAILURE);
    }
    // Print additional arguments (if any) as the error message.
    for (int i = 1; i < list->length - 1; i++) {
        fprintf(stderr, "%s ", list->data[i]);
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

/*
 * builtin_which, for executables only
 */
void builtin_which(arraylist_t *list) {
    int argCount = list->length - 1;
    if (argCount != 2) {  // "which" plus one argument
        fprintf(stderr, "which: expected one argument\n");
        return;
    }
    const char *cmd = list->data[1];
    const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    char path[4096];
    int found = 0;
    for (int i = 0; i < 3; i++) {
        
        strncpy(path, dirs[i], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';  // Ensure null-termination.
        strncat(path, "/", sizeof(path) - strlen(path) - 1);
        strncat(path, cmd, sizeof(path) - strlen(path) - 1);
        

        if (access(path, X_OK) == 0) {
            printf("%s\n", path);
            found = 1;
            break;
        }
    }
    if (!found)
        fprintf(stderr, "which: %s not found\n", cmd);
}
