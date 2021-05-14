#include <pv/set.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MIN_SPACE 4

bool resize(struct pv_set* set);

inline void* current_address(const struct pv_set* const set) {
    return set->array + (set->elem_size * set->current);
}

inline void* pos_address(const struct pv_set* const set, size_t pos) {
    return set->array + (set->elem_size * pos);
}

struct pv_set* pv_set_create(size_t elem_size, size_t init_space) {
    if(init_space <= MIN_SPACE) {
        init_space = MIN_SPACE;
    }

    struct pv_set* set = malloc(sizeof(struct pv_set));
    set->array = calloc(init_space, elem_size);
    set->max_size = init_space;
    set->current = 0;
    set->elem_size = elem_size;
    
    return set;
}

void pv_set_destroy(struct pv_set* const set) {
    assert(set != (void*)0);

    free(set->array);
    free(set);
}

bool pv_set_add(struct pv_set* const set, const void* datum) {
    assert(set != NULL);

    if(pv_set_contains(set, datum)) {
        return true;
    }

    if (set->current >= set->max_size) {
        if (!resize(set)) {
            // FIXME: return differently if fail
            return false;
        }
    }
    
    memcpy(current_address(set), datum, set->elem_size);
    set->current += 1;
    return true;
}

bool pv_set_contains(const struct pv_set* set, const void* datum) {
    assert(set != NULL);
    
    for(size_t i = 0; i < set->current; i += 1) {
        if(memcmp(current_address(set), datum, set->elem_size) == 0) {
            return true;
        }
    }
    
    return false;
}

bool pv_set_remove(struct pv_set* const set, const void* datum) {
    assert(set != NULL);
    
    for(size_t i = 0; i < set->current; i += 1) {
        if(memcmp(pos_address(set, i), datum, set->elem_size) == 0) {
            // Found. copy last to here and remove last
            memcpy(pos_address(set, i), current_address(set), set->elem_size);
            set->current -= 1;
            return true;
        }
    }
    
    return false;
}

bool resize(struct pv_set* set) {
    assert(set != NULL);
    size_t new_size = set->max_size * 2;
    void* new_array = realloc(set->array, set->elem_size * new_size);
    if (new_array == NULL) {
        return false;
    }
    
    set->array = new_array;
    set->max_size = new_size;
    return true;
}
