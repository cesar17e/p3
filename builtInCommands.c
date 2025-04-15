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
    if (chdir(list->data[1]) != 0) {  //chdir returns 0 if success
        perror("cd");
    }
}

/*
 * pwd - it expects only the command "pwd" (no extra arguments).
 */
void builtin_pwd(arraylist_t *list) {
    (void)list;  // Original code used list, but later on we realized we didnt need it, alot of our code uses it this way but its too much work to change it, just use (void)list it tells the compiler that were not using it on purpose
    char path[4096]; //The array for the path we wil get
    if (getcwd(path, sizeof(path)) == NULL) { 
        perror("built_pwd not working check");
        return;
    }
    printf("%s\n", path);
    fflush(stdout);  //Using flush instead of write
}

/*
 * builtin_exit--> it expects only the command exit
 */
void builtin_exit(arraylist_t *list) {
    int argCount = list->length - 1;
    if (argCount != 1) {
        fprintf(stderr, "exit: Does not expect arguments\n");
        return;
    }
    printf("mysh: exiting\n");
    fflush(stdout);  // Ensure the output is displayed, we use fflush not write the whole project
    exit(EXIT_SUCCESS);
}

/*
 * builtin_die:
 * Prints error messages following the die command, then exits with failure.
 */
void builtin_die(arraylist_t *list) {
    int argCount = list->length - 1;
    if (argCount < 1) {  // Should at least have die
        fprintf(stderr, "die: missing message\n");
        exit(EXIT_FAILURE);
    }
    // Print arguments as the error message.
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
    if (argCount != 2) {  // which needs a plus one argument
        fprintf(stderr, "which: expected more arguments\n");
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
        strncat(path, cmd, sizeof(path) - strlen(path) - 1); //So its like if we get ls we cat --> usr/bin/ls and then accesds will find it and then print it out to user
        

        if (access(path, X_OK) == 0) {
            printf("%s\n", path);
            found = 1;
            break;
        }
    }

    if (!found){ //If we didnt find it print something out
        fprintf(stderr, "which: %s not found\n", cmd);
    }
}