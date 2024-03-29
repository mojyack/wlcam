#pragma once
#include <string>

namespace ipu3 {
struct Args {
    const char* event_fifo = nullptr;
    const char* savedir    = nullptr;
    const char* cio2_devnode;
    const char* imgu_devnode;
    const char* cio2_entity;
    const char* imgu_entity;
    int         sensor_mbus_code = 0;
    int         sensor_width     = 0;
    int         sensor_height    = 0;
    int         width            = 0;
    int         height           = 0;
};

auto parse_args(int argc, const char* const argv[]) -> Args;
} // namespace ipu3
