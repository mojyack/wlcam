#pragma once
#include <cstdint>

#include "pixel-format.hpp"

struct Args {
    const char* video_device = "/dev/video0";
    const char* savedir      = nullptr;
    const char* event_fifo   = nullptr;
    uint32_t    width        = 1280;
    uint32_t    height       = 720;
    uint32_t    fps          = 30;
    PixelFormat pixel_format = PixelFormat::MPEG;
    double      button_x     = 1.0;
    double      button_y     = 0.5;
    bool        list_formats = false;
    bool        movie        = false;
};

auto parse_args(const int argc, const char* const argv[]) -> Args;
