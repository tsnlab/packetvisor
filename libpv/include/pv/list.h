#ifndef __PV_LIST_H__
#define __PV_LIST_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct pv_list {
    void* array;
    size_t elem_size;
    size_t max_size;
    size_t current;
};

struct pv_list* pv_list_create(size_t elem_size, size_t init_space);
void pv_list_destroy(struct pv_list* const list);
bool pv_list_append(struct pv_list* const list, const void* datum);
void* pv_list_get(const struct pv_list* list, size_t pos);
bool pv_list_del(struct pv_list* const list, size_t pos);

#endif
