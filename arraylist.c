#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "arraylist.h"

/* Set initial values of arraylist and allocate storage array.
*/
int al_init(arraylist_t *l, unsigned int cap) {
    assert(l != NULL);
    assert(cap > 0);
    l->data = malloc(cap * sizeof(char *));
    if (l->data == NULL) {
        return 1;
    } 
    l->capacity = cap;
    l->length = 0;
    return 0;
}
/* Free storage array.
* Assumes array is empty or contains references to unfreeable data.
*/
int al_destroy(arraylist_t *l){
    assert(l != NULL);
    free(l->data);
    return 0;
}
/* Free all strings in list and set length to 0.
*/
int al_clear(arraylist_t *l)
{
    assert(l != NULL);
    for (int i = 0; i < l->length; i++) free(l->data[i]);
    l->length = 0;
    return 0;
}
/* Add item to end of list, increasing storage as needed.
* Returns 0 for success, 1 if the storage could not be increased.
* Note: does not make a copy of the string.
*/

int al_append(arraylist_t *l, char *item) {
    assert(l != NULL);
    if (l->length == l->capacity) {
    // increase the underlying storage
        unsigned int newcap = l->capacity * 2;
        char **new = realloc(l->data, newcap * sizeof(char *));
        if (new == NULL) return 1;
            l->data = new;
            l->capacity = newcap;
        }
        l->data[l->length] = item;
        l->length++;
        return 0;
}
/* Removes last string from list, and assigns reference to dest.
* Returns 1 for success, 0 if the list is empty.
*/
int al_remove(arraylist_t *l, char **dest)
{
    assert(l != NULL);
    assert(dest != NULL);
    if (l->length == 0){
        return 0;
    }
    l->length--;
    *dest = l->data[l->length];
    return 1;
}
