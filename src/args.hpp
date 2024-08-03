#pragma once

struct CommonArgs {
    const char* savedir;
    int         width  = 1280;
    int         height = 720;

    // recoding
    const char* video_codec       = "libx264";
    const char* audio_codec       = "aac";
    const char* video_filter      = "";
    int         audio_sample_rate = 48000;

    bool ffmpeg_debug = false;
    bool help         = false;
};
