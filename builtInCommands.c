#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // For chdir(), getcwd(), access()
#include <string.h>
#include "builtInCommands.h"

void builtin_cd(arraylist_t *list) {
    // Expect list->data[0] to be "cd" and list->data[1] the directory.
    if (list->length != 2) {
        fprintf(stderr, "cd: expected one argument\n");
        return;
    }
    if (chdir(list->data[1]) != 0) {
        perror("cd");
    }
}

void builtin_pwd(arraylist_t *list) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return;
    }
    printf("%s\n", cwd);
}

void builtin_exit(arraylist_t *list) {
    // You could optionally ignore extra arguments.
    exit(EXIT_SUCCESS);
}

void builtin_die(arraylist_t *list) {
    // Print any error messages following "die" and exit with failure.
    for (int i = 1; i < list->length; i++) {
        fprintf(stderr, "%s ", list->data[i]);
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

void builtin_which(arraylist_t *list) {
    // For simplicity, assume exactly one argument: the command name.
    if (list->length != 2) {
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


// int is_builtin(const char *cmd) {
//     if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "die") == 0 || strcmp(cmd, "which") == 0){
//         return 1;
//     }
// }

// int handle_builtin(arraylist_t *list) {
//     if (list->length == 0)
//         return 0;  // No command to process, its empty for some reason maybe we need to find out why later
        
//     if (!is_builtin(list->data[0]))
//         return 0;  // Not a builtIn so the caller caller will need to use external stuff
    
//     // Compare and call the corresponding function
//     if (strcmp(list->data[0], "cd") == 0) {
//         builtin_cd(list);
//     } else if (strcmp(list->data[0], "pwd") == 0) {
//         builtin_pwd(list);
//     } else if (strcmp(list->data[0], "exit") == 0) {
//         builtin_exit(list);
//     } else if (strcmp(list->data[0], "die") == 0) {
//         builtin_die(list);
//     } else if (strcmp(list->data[0], "which") == 0) {
//         builtin_which(list);
//     }
    
//     return 1;  // Built-in was handled.
// }
