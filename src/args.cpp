#include <string_view>

#include "args.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"
#include "util/charconv.hpp"

auto ParseHelper::increment() -> bool {
    *index += 1;
    return *index < argc;
}

auto ParseHelper::get_int() -> std::optional<int> {
    assert_o(increment());
    unwrap_oo(num, from_chars<int>(argv[*index]));
    return num;
}

auto CommonArgs::parse(int argc, const char* const argv[]) -> std::optional<CommonArgs> {
    auto args   = CommonArgs();
    auto i      = 1;
    auto parser = ParseHelper{&i, argc, argv};

    for(i = 1; i < argc; i += 1) {
        const auto arg = std::string_view(argv[i]);
        if(arg == "-o" || arg == "--output") {
            assert_o(parser.increment());
            args.savedir = argv[i];
        } else if(arg == "--width") {
            unwrap_oo(num, parser.get_int());
            args.width = num;
        } else if(arg == "--height") {
            unwrap_oo(num, parser.get_int());
            args.height = num;
        } else if(arg == "--video-codec") {
            assert_o(parser.increment());
            args.video_codec = argv[i];
        } else if(arg == "--audio-codec") {
            assert_o(parser.increment());
            args.audio_codec = argv[i];
        } else if(arg == "--video-filter") {
            assert_o(parser.increment());
            args.video_filter = argv[i];
        } else if(arg == "--audio-sample-rate") {
            unwrap_oo(num, parser.get_int());
            args.audio_sample_rate = num;
        } else if(arg == "--ffmpeg-debug") {
            args.ffmpeg_debug = true;
        }
    }

    return args;
}
