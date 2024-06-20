#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include <sys/types.h>

namespace dev {
auto enumerate() -> std::optional<std::unordered_map<dev_t, std::string>>;
}
