#pragma once
#include <optional>

const auto common_flags_help = R"(  -o, --output PATH             output directory
  --width WIDTH                 horizontal resolution (1280)
  --height HEIGHT               vertical resolution (720)
  --video-codec CODEC           video codec for recording (see ffmpeg -codecs) (libx264)
  --audio-codec CODEC           audio codec for recording (see ffmpeg -codecs) (aac)
  --video-filter FILTER         video filter (null)
  --audio-sample-rate NUM       audio sampling rate (48000)
  --ffmpeg-debug                enable ffmpeg debug outputs)";

struct CommonArgs {
    const char* savedir;
    uint32_t    width  = 1280;
    uint32_t    height = 720;

    // recoding
    const char* video_codec       = "libx264";
    const char* audio_codec       = "aac";
    const char* video_filter      = "";
    uint32_t    audio_sample_rate = 48000;

    bool ffmpeg_debug = false;

    static auto parse(int argc, const char* const argv[]) -> std::optional<CommonArgs>;
};

struct ParseHelper {
    int*               index;
    int                argc;
    const char* const* argv;

    auto increment() -> bool;
    auto get_int() -> std::optional<int>;
};
