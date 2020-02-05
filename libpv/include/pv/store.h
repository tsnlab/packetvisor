#pragma once

#include <unordered_map>

namespace pv {
bool store_set_value(std::string key, void* value);
void* store_get_value(std::string key);
void store_erase_value(const std::string key);
} // namespace pv