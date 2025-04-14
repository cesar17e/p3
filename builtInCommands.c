#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // For chdir(), getcwd(), access()
#include <string.h>
#include "builtInCommands.h"

/*
 * builtin_cd:
 * Expects an argument list where list->data[0] is "cd" and list->data[1] is the directory.
 * Since finalizeArgs appends a NULL, the actual number of tokens is (list->length - 1).
 */
void builtin_cd(arraylist_t *list) {
    int argCount = list->length - 1;  // Exclude the NULL element
    if (argCount != 2) {  // We expect "cd" and one argument
        fprintf(stderr, "cd: expected one argument\n");
        return;
    }
    if (chdir(list->data[1]) != 0) {
        perror("cd");
    }
}

/*
 * builtin_pwd:
 * Expects only the command "pwd" (no extra arguments).
 */
void builtin_pwd(arraylist_t *list) {
    (void)list;  // pwd doesn't need extra arguments.
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return;
    }
    printf("%s\n", cwd);
    fflush(stdout);  // Ensure output is flushed immediately.
}

/*
 * builtin_exit:
 * Expects only the command "exit" (no extra arguments).
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
 * Expects at least "die" in the list.
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
 * builtin_which:
 * Expects an argument list where list->data[0] is "which" and list->data[1] is the command name.
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
        snprintf(path, sizeof(path), "%s/%s", dirs[i], cmd);
        if (access(path, X_OK) == 0) {
            printf("%s\n", path);
            found = 1;
            break;
        }
    }
    if (!found)
        fprintf(stderr, "which: %s not found\n", cmd);
}
