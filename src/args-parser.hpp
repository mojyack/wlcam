#include "args.hpp"
#include "util/argument-parser.hpp"

template <class... Args>
auto setup_common_args(CommonArgs& args, args::Parser<Args...>& parser) -> void {
    parser.kwarg(&args.savedir, {"-o", "--output"}, "PATH", "output directory");
    parser.kwarg(&args.width, {"--width"}, "WIDTH", "horizontal resolution", {.state = args::State::DefaultValue});
    parser.kwarg(&args.height, {"--height"}, "HEIGHT", "vertical resolution", {.state = args::State::DefaultValue});
    parser.kwarg(&args.video_codec, {"--video-codec"}, "CODEC", "video codec for recording(see ffmpeg -codecs)", {.state = args::State::DefaultValue});
    parser.kwarg(&args.audio_codec, {"--audio-codec"}, "CODEC", "audio codec for recording(see ffmpeg -codecs)", {.state = args::State::DefaultValue});
    parser.kwarg(&args.video_filter, {"--video-filter"}, "FILTER", "video filter", {.state = args::State::Initialized});
    parser.kwarg(&args.audio_sample_rate, {"--audio-sample-rate"}, "NUM", "audio sampling rate", {.state = args::State::DefaultValue});
    parser.kwflag(&args.ffmpeg_debug, {"--ffmpeg-debug"}, "enable ffmpeg debug outputs");
    parser.kwflag(&args.help, {"-h", "--help"}, "print this help message", {.no_error_check = true});
}
