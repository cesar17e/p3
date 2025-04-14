#ifndef BUILTINS_H //The guards
#define BUILTINS_H

#include "arraylist.h"
void builtin_cd(arraylist_t *list);
void builtin_pwd(arraylist_t *list);
void builtin_exit(arraylist_t *list);
void builtin_die(arraylist_t *list);
void builtin_which(arraylist_t *list);

#endif 