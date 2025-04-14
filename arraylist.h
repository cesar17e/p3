#ifndef ARRAYLIST_H
#define ARRAYLIST_H

typedef struct {
    char **data;
    unsigned int capacity;
    unsigned int length;
} arraylist_t;

int al_init(arraylist_t *list, unsigned int cap);
int al_destroy(arraylist_t *list);
int al_clear(arraylist_t *list);
int al_append(arraylist_t *list, char *item);
int al_remove(arraylist_t *list, char **item);

#endif 
