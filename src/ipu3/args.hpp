#pragma once
#include <optional>
#include <vector>

#include "../args.hpp"

namespace ipu3 {
struct Args : CommonArgs {
    using Params = std::vector<std::pair<std::string_view, int>>;

    const char* cio2_devnode;
    const char* imgu_devnode;
    const char* cio2_entity;
    const char* imgu_entity;
    int         sensor_mbus_code;
    int         sensor_width;
    int         sensor_height;
    Params      ipu3_params;

    static auto parse(int argc, const char* const argv[]) -> std::optional<Args>;
};
} // namespace ipu3
