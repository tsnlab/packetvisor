#ifndef __PV_LIST_H__
#define __PV_LIST_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct pv_set {
    void* array;
    size_t elem_size;
    size_t max_size;
    size_t current;
};

struct pv_set* pv_set_create(size_t elem_size, size_t init_space);
void pv_set_destroy(struct pv_set* const list);
bool pv_set_add(struct pv_set* const list, const void* datum);
bool pv_set_remove(struct pv_set* const list, const void* datum);
bool pv_set_contains(const struct pv_set* list, const void* datum);

#endif
