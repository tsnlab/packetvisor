#include <pv/list.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MIN_SPACE 4

bool resize(struct pv_list* list);

struct pv_list* pv_list_create(size_t elem_size, size_t init_space) {
    if(init_space <= MIN_SPACE) {
        init_space = MIN_SPACE;
    }

    struct pv_list* list = malloc(sizeof(struct pv_list));
    list->array = calloc(init_space, elem_size);
    list->max_size = init_space;
    list->current = 0;
    list->elem_size = elem_size;
    
    return list;
}

void pv_list_destroy(struct pv_list* const list) {
    assert(list != (void*)0);

    free(list->array);
    free(list);
}

bool pv_list_append(struct pv_list* const list, const void* datum) {
    assert(list != NULL);
    if (list->current >= list->max_size) {
        if (!resize(list)) {
            return false;
        }
    }
    
    memcpy(list->array + (list->elem_size * list->current), datum, list->elem_size);
    list->current += 1;
    return true;
}

void* pv_list_get(const struct pv_list* list, size_t pos) {
    assert(list != NULL);
    if (list->current < pos) {
        return NULL;
    }
    
    return list->array + (list->elem_size * pos);
}

bool pv_list_del(struct pv_list* const list, size_t pos) {
    assert(list != NULL);
    
    if (list->current < pos) {
        return false;
    }
    
    memmove(
        list->array + (list->elem_size * pos),
        list->array + (list->elem_size * (pos + 1)),
        list->elem_size * (list->current - pos));
        
    list->current -= 1;
    
    return true;
}

bool resize(struct pv_list* list) {
    assert(list != NULL);
    size_t new_size = list->max_size * 2;
    void* new_array = realloc(list->array, list->elem_size * new_size);
    if (new_array == NULL) {
        return false;
    }
    
    list->array = new_array;
    list->max_size = new_size;
    return true;
}
