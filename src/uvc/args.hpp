#pragma once
#include "../args.hpp"
#include "../v4l2.hpp"

struct Args : CommonArgs {
    struct FourCC {
        uint32_t data;
    };

    const char* video_device = "/dev/video0";
    int         fps          = 30;
    FourCC      pixel_format = {v4l2::fourcc("MJPG")};
    bool        list_formats = false;

    static auto parse(int argc, const char* const argv[]) -> std::optional<Args>;
};

