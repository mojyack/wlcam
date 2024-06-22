#include <string_view>

#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "../util/charconv.hpp"
#include "args.hpp"

namespace {
const auto help = R"str(usage: wl-cam [FLAG|OPTION]...
flags:
  -h, --help                    print this message
  -l, --list-formats            list supported formats of the video device
  -m, --movie                   enable movie mode
  --ffmpeg-debug                enable ffmpeg debug output
options:
  -d, --device PATH             video device (/dev/video0)
  -s, --server PATH             create fifo for companion
  -o, --output PATH             output directory
  --width WIDTH                 horizontal resolution (1280)
  --height HEIGHT               vertical resolution (720)
  --fps FPS                     refresh rate (30)
  --pix-format {MJPG|YUYV|NV12} pixel format (MPEG)
  --video-codec CODEC           video codec for recording (see ffmpeg -codecs) (libx264)
  --audio-codec CODEC           audio codec for recording (see ffmpeg -codecs) (aac)
  --video-filter FILTER         video filter (null)
  --audio-sample-rate NUM       audio sampling rate (48000)
)str";
}

auto parse_args(const int argc, const char* const argv[]) -> std::optional<Args> {
    auto args = Args();

    const auto increment = [argc](int& i) -> bool {
        i += 1;
        return i < argc;
    };

    for(auto i = 1; i < argc; i += 1) {
        const auto arg = std::string_view(argv[i]);
        if(arg == "-h" || arg == "--help") {
            print(help);
            exit(0);
        } else if(arg == "-l" || arg == "--list_formats") {
            args.list_formats = true;
        } else if(arg == "-m" || arg == "--movie") {
            args.movie = true;
        } else if(arg == "--ffmpeg-debug") {
            args.ffmpeg_debug = true;
        } else if(arg == "-d" || arg == "--device") {
            assert_o(increment(i));
            args.video_device = argv[i];
        } else if(arg == "-s" || arg == "--server") {
            assert_o(increment(i));
            args.event_fifo = argv[i];
        } else if(arg == "-o" || arg == "--output") {
            assert_o(increment(i));
            args.savedir = argv[i];
        } else if(arg == "--width") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.width = num;
        } else if(arg == "--height") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.height = num;
        } else if(arg == "--fps") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.fps = num;
        } else if(arg == "--pix-format") {
            assert_o(increment(i));
            assert_o(strlen(argv[i]) == 4, "invalid pixel format");
            args.pixel_format = v4l2::fourcc(argv[i]);
        } else if(arg == "--video-codec") {
            assert_o(increment(i));
            args.video_codec = argv[i];
        } else if(arg == "--audio-codec") {
            assert_o(increment(i));
            args.audio_codec = argv[i];
        } else if(arg == "--video-filter") {
            assert_o(increment(i));
            args.video_filter = argv[i];
        } else if(arg == "--audio-sample-rate") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.audio_sample_rate = num;
        }
    }

    assert_o(args.savedir != nullptr, "no --output argument");

    return args;
}
