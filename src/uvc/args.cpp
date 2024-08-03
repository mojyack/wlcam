#include <string_view>

#include "../args-parser.hpp"
#include "../macros/assert.hpp"
#include "args.hpp"

namespace args {
namespace {
template <>
auto from_string<Args::FourCC>(const char* str) -> std::optional<Args::FourCC> {
    assert_o(strlen(str) == 4);
    return Args::FourCC{v4l2::fourcc(str)};
}

template <>
auto to_string<Args::FourCC>(const Args::FourCC& data) -> std::string {
    auto ret = std::string(4, '\0');

    *std::bit_cast<uint32_t*>(ret.data()) = data.data;

    return ret;
}
} // namespace
} // namespace args

auto Args::parse(const int argc, const char* const argv[]) -> std::optional<Args> {
    auto args   = Args();
    auto parser = args::Parser<FourCC>();
    setup_common_args(args, parser);
    parser.kwarg(&args.video_device, {"-d", "--device"}, {"PATH", "video device", args::State::DefaultValue});
    parser.kwarg(&args.fps, {"--fps"}, {"FPS", "refresh rate", args::State::DefaultValue});
    parser.kwarg(&args.pixel_format, {"--pix-format"}, {"{MJPG|YUYV|NV12}", "pixel format", args::State::DefaultValue});
    parser.kwarg(&args.list_formats, {"-l", "--list-formats"}, {"", "list supported formats of the video device", args::State::Initialized});
    if(!parser.parse(argc, argv) || args.help) {
        print("usage: wlcam-uvc ", parser.get_help());
        return std::nullopt;
    }
    return args;
}
