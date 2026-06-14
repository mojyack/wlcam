#include <print>

#include "../args-parser.hpp"
#include "../macros/assert.hpp"
#include "args.hpp"

namespace camss {
auto Args::parse(const int argc, const char* const* argv) -> std::optional<Args> {
    auto args   = Args();
    auto parser = args::Parser<uint8_t, uint16_t>();
    parser.kwarg(&args.media_device, {"-m", "--media"}, "PATH", "media device", {.state = args::State::DefaultValue});
    parser.kwarg(&args.csiphy, {"--csiphy"}, "N", "csiphy index");
    parser.kwarg(&args.csid, {"--csid"}, "N", "csid index", {.state = args::State::DefaultValue});
    parser.kwarg(&args.vfe, {"--vfe"}, "N", "vfe index", {.state = args::State::DefaultValue});
    parser.kwarg(&args.cell, {"--cell"}, "N", "quad-bayer support", {.state = args::State::DefaultValue});
    parser.kwarg(&args.width, {"--width"}, "WIDTH", "horizontal resolution", {.state = args::State::DefaultValue});
    parser.kwarg(&args.height, {"--height"}, "HEIGHT", "vertical resolution", {.state = args::State::DefaultValue});
    parser.kwarg(&args.gamma, {"--gamma"}, "EXP", "display gamma exponent", {.state = args::State::DefaultValue});
    parser.kwarg(&args.black_level, {"--black"}, "LEVEL", "black level, 0..1", {.state = args::State::DefaultValue});
    parser.kwarg(&args.wb_r, {"--wb-r"}, "GAIN", "red white-balance gain", {.state = args::State::DefaultValue});
    parser.kwarg(&args.wb_g, {"--wb-g"}, "GAIN", "green white-balance gain", {.state = args::State::DefaultValue});
    parser.kwarg(&args.wb_b, {"--wb-b"}, "GAIN", "blue white-balance gain", {.state = args::State::DefaultValue});
    parser.kwarg(&args.lsc, {"--lsc"}, "STRENGTH", "lens shading correction strength", {.state = args::State::DefaultValue});
    parser.kwarg(&args.rotate, {"--rotate"}, "DEG", "rotate the image clockwise: 0, 90, 180 or 270", {.state = args::State::DefaultValue});
    parser.kwarg(&args.savedir, {"-o", "--output"}, "PATH", "output directory for photos/videos", {.state = args::State::DefaultValue});
    // parser.kwarg(&args.video_codec, {"--video-codec"}, "CODEC", "video codec for recording (see ffmpeg -codecs)", {.state = args::State::DefaultValue});
    parser.kwarg(&args.audio_codec, {"--audio-codec"}, "CODEC", "audio codec for recording (see ffmpeg -codecs)", {.state = args::State::DefaultValue});
    parser.kwarg(&args.video_filter, {"--video-filter"}, "FILTER", "video filter", {.state = args::State::Initialized});
    parser.kwarg(&args.audio_sample_rate, {"--audio-sample-rate"}, "NUM", "audio sampling rate", {.state = args::State::DefaultValue});
    parser.kwflag(&args.ffmpeg_debug, {"--ffmpeg-debug"}, "enable ffmpeg debug outputs");
    parser.kwflag(&args.help, {"-h", "--help"}, "print this help message", {.no_error_check = true});
    if(!parser.parse(argc, argv) || args.help) {
        std::println("usage: wlcam-camss {}", parser.get_help());
        exit(0);
    }
    ensure(args.rotate % 90 == 0);
    return args;
}
} // namespace camss
