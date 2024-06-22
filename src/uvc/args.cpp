#include <string_view>

#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "args.hpp"

namespace {
const auto help = R"(usage: wl-cam OPTIONS...
options:
  -h, --help                    print this message
  -l, --list-formats            list supported formats of the video device
  -d, --device PATH             video device (/dev/video0)
  --fps FPS                     refresh rate (30)
  --pix-format {MJPG|YUYV|NV12} pixel format (MPEG)
)";
}

auto Args::parse(const int argc, const char* const argv[]) -> std::optional<Args> {
    unwrap_oo(common, CommonArgs::parse(argc, argv));

    auto args   = Args(common);
    auto i      = 1;
    auto parser = ParseHelper{&i, argc, argv};

    for(i = 1; i < argc; i += 1) {
        const auto arg = std::string_view(argv[i]);
        if(arg == "-h" || arg == "--help") {
            print(help, common_flags_help);
            exit(0);
        } else if(arg == "-d" || arg == "--device") {
            assert_o(parser.increment());
            args.video_device = argv[i];
        } else if(arg == "--fps") {
            unwrap_oo(num, parser.get_int());
            args.fps = num;
        } else if(arg == "--pix-format") {
            assert_o(parser.increment());
            assert_o(strlen(argv[i]) == 4, "invalid pixel format");
            args.pixel_format = v4l2::fourcc(argv[i]);
        } else if(arg == "-l" || arg == "--list_formats") {
            args.list_formats = true;
        }
    }

    assert_o(args.list_formats || args.savedir != nullptr, "no --output argument");

    return args;
}
