#pragma once
#include <cstdint>

#include "../args.hpp"
#include "../v4l2.hpp"

struct Args : CommonArgs {
    const char* video_device = "/dev/video0";
    uint32_t    fps          = 30;
    uint32_t    pixel_format = v4l2::fourcc("MJPG");
    bool        list_formats = false;

    static auto parse(int argc, const char* const argv[]) -> std::optional<Args>;
};

