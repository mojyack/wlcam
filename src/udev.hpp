#pragma once
#include <string>
#include <unordered_map>

#include <sys/types.h>

namespace dev {
auto enumerate() -> std::unordered_map<dev_t, std::string>;
}
