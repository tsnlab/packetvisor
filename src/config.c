#include <stdlib.h>
#include <assert.h>

#include "zf_log.h"

#include <pv/config.h>

#define ENV_NAME "PV_CONFIG"

struct pv_config* pv_config_create() {
    struct pv_config* config = malloc(sizeof(struct pv_config));
    if (config == NULL) {
        ZF_LOGE("Cannot allocate memory");
        return NULL;
    }

    // TODO: Implement this
    ZF_LOGI("Config using address %p", config);

    return config;
}

void pv_config_destroy(struct pv_config* config) {
    assert(config != NULL);

    free(config);

    // TODO: free all child components
}
