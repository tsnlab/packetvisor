#include <pv/store.h>

#include <tbb/concurrent_unordered_map.h>

namespace pv {
static tbb::concurrent_unordered_map<std::string, void*> data_store;

bool store_set_value(std::string key, void* value) {
    auto it = data_store.insert(std::make_pair(key, value));
    return it.second;
}

void* store_get_value(const std::string key) {
    auto it = data_store.find(key);
    if(it == data_store.end())
        return NULL;
    
    return it->second;
}

void store_erase_value(const std::string key) {
    data_store.unsafe_erase(key);
}
} // namespace pv