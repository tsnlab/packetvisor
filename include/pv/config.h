#pragma once

#include <netinet/in.h>

#include <stdbool.h>
#include <stdint.h>

struct pv_config_memory {
    uint32_t packet_pool;   // Count
    uint32_t shared_memory; // Bytes
};

struct pv_config_nic {
    char* dev;     // *required* pci port num 0000:00:08.0
    uint64_t mac;  // c0:ff:ee:c0:ff:ee or NULL
    struct in_addr ipv4; // 192.168.0.1, NULL if auto
    struct in6_addr ipv6;  // ::1, NULL if auto
    uint16_t rx_queue;
    uint16_t tx_queue;
    uint32_t rx_offloads;
    uint32_t tx_offloads;
    bool promisc;
};

enum pv_config_loglevel {
    PV_LOGLEVEL_VERBOSE,
    PV_LOGLEVEL_DEBUG,
    PV_LOGLEVEL_INFO,
    PV_LOGLEVEL_WARNING,
    PV_LOGLEVEL_ERROR,
};

enum pv_config_type {
    PV_CONFIG_UNKNOWN,  // Not found, or unknown
    PV_CONFIG_BOOL,
    PV_CONFIG_NUM,
    PV_CONFIG_STR,
    PV_CONFIG_DICT,
    PV_CONFIG_LIST,
};

struct pv_config {
    uint16_t* cores;
    uint16_t cores_count;

    size_t eal_argc;
    char** eal_argv;

    struct pv_config_memory memory;

    struct pv_config_nic* nics;
    uint16_t nics_count;

    uint8_t loglevel; // See pv_config_loglevel
};

struct pv_config* pv_config_create();
void pv_config_destroy(struct pv_config* config);
void pv_config_finalize();

/**
 * Get cores count that specified on config file.
 * @return count of cores
 */
size_t pv_config_get_core_count();

/**
 * Get cores list from config file.
 * @param cores array of int to store core ids.
 * @param max_count maximum count to store core ids. if there are more than this value. they are ignored. @see pv_config_get_core_count
 * @return count of cores that is returned from this function.
 */
size_t pv_config_get_cores(int* cores, size_t max_count);

// Below are for custom configs

/**
 * Check if config has value with given key.
 * @param key key to look for
 * @return true if found
 */
bool pv_config_has(const char* key);

/**
 * Get type of config value with given key.
 * @param key key to look for
 * @return type of config value. @see enum pv_config_type
 */
enum pv_config_type pv_config_get_type(const char* key);
/**
 * Get length of config value for dict/list type.
 * @param key key to look for
 * @return size of config. or -1 if key doesn't exists or not suitable type.
 */
size_t pv_config_get_size(const char* key);

/**
 * Get the key name of given dict.
 * @param dict key of dict object.
 * @param key_index 0 based key index. @see pv_config_get_size
 * @return key's name
 */
char* pv_config_get_key(const char* dict, int key_index);

/**
 * Get string value of config
 * @param key key to look for
 * @return a string. null if not found. @see pv_config_has
 */
char* pv_config_get_str(const char* key);

/**
 * Get string value of config
 * @param key key to look for
 * @return a number. -1 if not found. @see pv_config_has
 */
int pv_config_get_num(const char* key);

/**
 * Get bool value of config
 * @param key key to look for
 * @return a bool. false if not found. @see pv_config_has
 */
bool pv_config_get_bool(const char* key);
