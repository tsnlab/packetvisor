#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <cl/collection.h>
#include <cl/map.h>

#include "zf_log.h"

#include <pv/config.h>

#define ENV_NAME "PV_CONFIG"
#define MAX_LINE_LENGTH 200

struct pv_config* pv_config_create() {
    struct pv_config* config = malloc(sizeof(struct pv_config));
    if(config == NULL) {
        ZF_LOGE("Cannot allocate memory");
        return NULL;
    }

    // TODO: Implement this
    struct map* config_map = map_create(10, string_hash, string_compare);
    FILE* config_file = fopen(getenv(ENV_NAME), "r");
    char line[MAX_LINE_LENGTH];
    while(fgets(line, MAX_LINE_LENGTH, config_file) != NULL) {
        char* space = strchr(line, ' ');
        assert(space != NULL);

        *space = '\0';
        char* value = space + 1;

        // Remove last \n charactor
        char* nr = strchr(value, '\n');
        if(nr != NULL) {
            *nr = '\0';
        }

        char* mapkey = strdup(line);
        char* mapval = strdup(value);

        ZF_LOGD("Put config %s with value %s", mapkey, mapval);
        map_put(config_map, mapkey, mapval);
    }

    config->config_map = config_map;
    return config;
}

void pv_config_destroy(struct pv_config* config) {
    assert(config != NULL);

    if(config->config_map != NULL) {
        struct map_iterator iter;
        map_iterator_init(&iter, config->config_map);
        while(map_iterator_has_next(&iter)) {
            struct map_entry* entry = map_iterator_next(&iter);
            ZF_LOGD("Remove config %s %s", (char*)entry->key, (char*)entry->data);
            free(entry->key);
            free(entry->data);
        }

        map_destroy(config->config_map);
    }

    free(config);

    // TODO: free all child components
}
