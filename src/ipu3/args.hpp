#pragma once
#include <optional>
#include <vector>

#include "../args.hpp"

namespace ipu3 {
struct Args : CommonArgs {
    using Params = std::vector<std::pair<std::string_view, int>>;

    std::string_view cio2_devnode;
    std::string_view imgu_devnode;
    std::string_view cio2_entity;
    std::string_view imgu_entity;
    int              sensor_mbus_code;
    int              sensor_width;
    int              sensor_height;
    Params           ipu3_params;

    static auto parse(int argc, const char* const argv[]) -> std::optional<Args>;
};
} // namespace ipu3
