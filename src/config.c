#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

// #include <cl/collection.h>
#include <cl/map.h>

#include "zf_log.h"

#include <pv/config.h>

#define ENV_NAME "PV_CONFIG"
#define MAX_LINE_LENGTH 200

static struct map* config_map = NULL;

static bool create_config();
static void fill_pv_config(struct pv_config* config);

struct pv_config* pv_config_create() {
    struct pv_config* config = malloc(sizeof(struct pv_config));
    if(config == NULL) {
        ZF_LOGE("Cannot allocate memory");
        return NULL;
    }

    create_config();

    fill_pv_config(config);

    return config;
}

static bool create_config() {
    if(config_map != NULL) {
        return true;
    }

    config_map = map_create(10, string_hash, string_compare);
    FILE* config_file = fopen(getenv(ENV_NAME), "r");
    // TODO: check file
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

        ZF_LOGV("Put config %s with value %s", mapkey, mapval);
        map_put(config_map, mapkey, mapval);
    }

    return true;
}

static void fill_pv_config(struct pv_config* config) {
    struct map* map = config_map;

    if(map_has(map, "/eal_params/:type")) {
        config->eal_argc = atoi(map_get(map, "/eal_params/:length"));
        config->eal_argv = calloc(config->eal_argc, sizeof(char*));

        for(size_t i = 0; i < config->eal_argc; i += 1) {
            char key[200];
            snprintf(key, sizeof(key) / sizeof(char), "/eal_params[%lu]/", i);
            config->eal_argv[i] = map_get(map, key);
        }
    }

    if(map_has(map, "/cores/:type")) {
        config->cores_count = atoi(map_get(map, "/cores/:length"));
        config->cores = calloc(config->cores_count, sizeof(config->cores[0]));
        assert(config->cores);

        for(size_t i = 0; i < config->cores_count; i += 1) {
            char key[200];
            snprintf(key, sizeof(key) / sizeof(char), "/cores[%lu]/", i);
            config->cores[i] = atoi(map_get(map, key));
        }
    }

    if(map_has(map, "/memory/:type")) {
        if(map_has(map, "/memory/packet_pool/")) {
            config->memory.packet_pool = atoi(map_get(map, "/memory/packet_pool/"));
            ZF_LOGD("packet pool: %d", config->memory.packet_pool);
        }

        if(map_has(map, "/memory/shared_memory/")) {
            config->memory.shared_memory = atoi(map_get(map, "/memory/shared_memory/"));
            ZF_LOGD("shared memory: %d", config->memory.shared_memory);
        }
    }

    if(map_has(map, "/log_level/:type")) {
        char* log_level = (char*)map_get(map, "/log_level/");

        if(strcasecmp(log_level, "verbose") == 0) {
            config->loglevel = PV_LOGLEVEL_VERBOSE;
        } else if(strcasecmp(log_level, "debug") == 0) {
            config->loglevel = PV_LOGLEVEL_DEBUG;
        } else if(strcasecmp(log_level, "info") == 0) {
            config->loglevel = PV_LOGLEVEL_INFO;
        } else if(strcasecmp(log_level, "warning") == 0) {
            config->loglevel = PV_LOGLEVEL_WARNING;
        } else if(strcasecmp(log_level, "error") == 0) {
            config->loglevel = PV_LOGLEVEL_ERROR;
        } else {
            ZF_LOGE("log level %s is not available", log_level);
        }
    }

    if(map_has(map, "/nics/:type")) {
        config->nics_count = atoi(map_get(map, "/nics/:length"));
        config->nics = calloc(config->nics_count, sizeof(config->nics[0]));
        assert(config->nics != NULL);
        char key[200];

        for(size_t i = 0; i < config->nics_count; i += 1) {
            snprintf(key, 200, "/nics[%lu]/dev/", i);
            char* dev = map_get(map, key);
            config->nics[i].dev = dev;

            snprintf(key, 200, "/nics[%lu]/mac/", i);
            char* mac = map_get(map, key);
            if(mac != NULL) {
                config->nics[i].mac = (((strtoul(mac + (0 * 3), NULL, 16) & 0xff) << (8 * 5)) |
                                       ((strtoul(mac + (1 * 3), NULL, 16) & 0xff) << (8 * 4)) |
                                       ((strtoul(mac + (2 * 3), NULL, 16) & 0xff) << (8 * 3)) |
                                       ((strtoul(mac + (3 * 3), NULL, 16) & 0xff) << (8 * 2)) |
                                       ((strtoul(mac + (4 * 3), NULL, 16) & 0xff) << (8 * 1)) |
                                       ((strtoul(mac + (5 * 3), NULL, 16) & 0xff) << (8 * 0)));

                ZF_LOGD("nic[%lu]/mac: %012lx", i, config->nics[i].mac);
            }

            snprintf(key, 200, "/nics[%lu]/ipv4/", i);
            char* ipv4 = map_get(map, key);
            if(ipv4 != NULL) {
                struct sockaddr_in addr;
                inet_pton(AF_INET, ipv4, &addr);
                memcpy(&config->nics[i].ipv4, &addr.sin_addr, sizeof(addr.sin_addr));
            }

            snprintf(key, 200, "/nics[%lu]/ipv6/", i);
            char* ipv6 = map_get(map, key);
            if(ipv6 != NULL) {
                struct sockaddr_in6 addr;
                inet_pton(AF_INET, ipv6, &addr);
                memcpy(&config->nics[i].ipv6, &addr.sin6_addr, sizeof(addr.sin6_addr));
            }

            snprintf(key, 200, "/nics[%lu]/rx_queue/", i);
            char* rx_queue = map_get(map, key);
            if(rx_queue != NULL) {
                config->nics[i].rx_queue = atoi(rx_queue);
            }

            snprintf(key, 200, "/nics[%lu]/tx_queue/", i);
            char* tx_queue = map_get(map, key);
            if(tx_queue != NULL) {
                config->nics[i].tx_queue = atoi(tx_queue);
            }

            // TODO: Offload flags

            snprintf(key, 200, "/nics[%lu]/promisc/", i);
            char* promisc = map_get(map, key);
            // Assume that promisc in ("0", "1")
            if(promisc != NULL) {
                config->nics[i].promisc = (promisc[0] == '1');
            }
        }
    }
}

void pv_config_destroy(struct pv_config* config) {
    assert(config != NULL);

    if(config->nics) {
        free(config->nics);
    }

    if(config->cores) {
        free(config->cores);
    }

    free(config);
}

void pv_config_finalize() {
    if(config_map != NULL) {
        struct map_iterator iter;
        map_iterator_init(&iter, config_map);
        while(map_iterator_has_next(&iter)) {
            struct map_entry* entry = map_iterator_next(&iter);
            ZF_LOGV("Remove config %s %s", (char*)entry->key, (char*)entry->data);
            free(entry->key);
            free(entry->data);
        }

        map_destroy(config_map);
    }
}

size_t pv_config_get_core_count() {
    create_config();
    struct map* map = config_map;
    if(!map_has(map, "/cores/:type")) {
        return 0;
    }
    return atoi(map_get(map, "/cores/:length"));
}

size_t pv_config_get_cores(int* cores, size_t max_count) {

    assert(cores != NULL);

    int cores_count = pv_config_get_core_count();
    struct map* map = config_map;

    size_t i;
    for(i = 0; i < cores_count && i < max_count; i += 1) {
        char key[200];
        snprintf(key, sizeof(key) / sizeof(char), "/cores[%lu]/", i);
        cores[i] = atoi(map_get(map, key));
    }

    return i;
}

// Below are for custom configs

bool pv_config_has(const char* key) {
    return pv_config_get_type(key) != PV_CONFIG_UNKNOWN;
}

enum pv_config_type pv_config_get_type(const char* key) {
    size_t key_type_size = strlen(key) + strlen("/:type") + 1;
    char key_type[key_type_size];
    snprintf(key_type, key_type_size, "%s/:type", key);

    char* type = map_get(config_map, key_type);

    if (type == NULL) {
        return PV_CONFIG_UNKNOWN;
    }

    if (strcasecmp(type, "str") == 0) {
        return PV_CONFIG_STR;
    } else if (strcasecmp(type, "num") == 0) {
        return PV_CONFIG_NUM;
    } else if (strcasecmp(type, "bool") == 0) {
        return PV_CONFIG_BOOL;
    } else if (strcasecmp(type, "dict") == 0) {
        return PV_CONFIG_DICT;
    } else if (strcasecmp(type, "list") == 0) {
        return PV_CONFIG_LIST;
    }

    return PV_CONFIG_UNKNOWN;
}

size_t pv_config_get_size(const char* key) {
    size_t key_length_size = strlen(key) + strlen("/:length") + 1;
    char key_length[key_length_size];
    snprintf(key_length, key_length_size, "%s/:length", key);

    char* length = map_get(config_map, key_length);

    if (length == NULL) {
        return -1;
    }

    return atoi(length);
}

char* pv_config_get_key(const char* dict, int key_index) {
    if (pv_config_get_type(dict) != PV_CONFIG_DICT) {
        return NULL;
    }

    const size_t key_value_size = strlen(dict) + strlen("/:keys[xx]") + 1;
    char key_value[key_value_size];

    snprintf(key_value, key_value_size, "%s/:keys[%d]", dict, key_index);
    char* value = map_get(config_map, key_value);

    return value;
}

char* pv_config_get_str(const char* key) {
    create_config();

    // Check type
    if (pv_config_get_type(key) != PV_CONFIG_STR) {
        return NULL;
    }

    const size_t key_value_size = strlen(key) + 1 + 1;
    char key_value[key_value_size];
    snprintf(key_value, key_value_size, "%s/", key);

    char* value = map_get(config_map, key_value);
    return value;
}

int pv_config_get_num(const char* key) {
    create_config();

    // Check type
    if (pv_config_get_type(key) != PV_CONFIG_NUM) {
        return -1;
    }

    const size_t key_value_size = strlen(key) + 1 + 1;
    char key_value[key_value_size];
    snprintf(key_value, key_value_size, "%s/", key);

    char* value = map_get(config_map, key_value);
    if (value == NULL) {
        return -1;
    }

    return atoi(value);
}

bool pv_config_get_bool(const char* key) {
    create_config();

    // Check type
    if (pv_config_get_type(key) != PV_CONFIG_BOOL) {
        return false;
    }

    const size_t key_value_size = strlen(key) + 1 + 1;
    char key_value[key_value_size];
    snprintf(key_value, key_value_size, "%s/", key);

    char* value = map_get(config_map, key_value);
    if (value == NULL) {
        return false;
    }

    return strcasecmp(value, "1") == 0;
}
