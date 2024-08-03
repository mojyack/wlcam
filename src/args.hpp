#pragma once
#include <string_view>

struct CommonArgs {
    std::string_view savedir;
    int              width  = 1280;
    int              height = 720;

    // recoding
    std::string_view video_codec       = "libx264";
    std::string_view audio_codec       = "aac";
    std::string_view video_filter      = "";
    int              audio_sample_rate = 48000;

    bool ffmpeg_debug = false;
    bool help         = false;
};
