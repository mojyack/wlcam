#pragma once
#include <string>

namespace ipu3 {
struct Args {
    std::string cio2_devnode;
    std::string imgu_devnode;
    std::string cio2_entity;
    std::string imgu_entity;
    int         sensor_mbus_code = 0;
    int         sensor_width     = 0;
    int         sensor_height    = 0;
    int         width            = 0;
    int         height           = 0;
};

auto parse_args(int argc, const char* const argv[]) -> Args;
} // namespace ipu3
