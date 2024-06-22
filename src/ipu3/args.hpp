#pragma once
#include <optional>
#include <vector>

#include "../args.hpp"

namespace ipu3 {
struct Args : CommonArgs {
    const char* cio2_devnode;
    const char* imgu_devnode;
    const char* cio2_entity;
    const char* imgu_entity;
    int         sensor_mbus_code = 0;
    int         sensor_width     = 0;
    int         sensor_height    = 0;

    std::vector<std::pair<const char*, int>> ipu3_params;

    static auto parse(int argc, const char* const argv[]) -> std::optional<Args>;
};
} // namespace ipu3
