#include "args.hpp"
#include "util/argument-parser.hpp"

template <class... Args>
auto setup_common_args(CommonArgs& args, args::Parser<Args...>& parser) -> void {
    parser.kwarg(&args.savedir, {"-o", "--output"}, {"PATH", "output directory", args::State::Uninitialized});
    parser.kwarg(&args.width, {"--width"}, {"WIDTH", "horizontal resolution", args::State::DefaultValue});
    parser.kwarg(&args.height, {"--height"}, {"HEIGHT", "vertical resolution", args::State::DefaultValue});
    parser.kwarg(&args.video_codec, {"--video-codec"}, {"CODEC", "video codec for recording(see ffmpeg -codecs)", args::State::DefaultValue});
    parser.kwarg(&args.audio_codec, {"--audio-codec"}, {"CODEC", "audio codec for recording(see ffmpeg -codecs)", args::State::DefaultValue});
    parser.kwarg(&args.video_filter, {"--video-filter"}, {"FILTER", "video filter", args::State::Initialized});
    parser.kwarg(&args.audio_sample_rate, {"--audio-sample-rate"}, {"NUM", "audio sampling rate", args::State::DefaultValue});
    parser.kwarg(&args.ffmpeg_debug, {"--ffmpeg-debug"}, {"", "enable ffmpeg debug outputs", args::State::Initialized});
    parser.kwarg(&args.help, {"-h", "--help"}, {"", "print this help message", args::State::Initialized});
}
