#include <string_view>

#include "args.hpp"
#include "util/charconv.hpp"
#include "util/error.hpp"

namespace {
const auto help = R"str(usage: wl-cam [FLAG|OPTION]...
flags:
  -h, --help                print this message
  -l, --list-formats        list supported formats of the video device
  -m, --movie               enable movie mode               
options:
  -d, --device PATH         video device (/dev/video0)
  -s, --server PATH         create fifo for companion
  -o, --output PATH         output directory
  --width WIDTH             horizontal resolution (1280)
  --height HEIGHT           vertical resolution (720)
  --fps FPS                 refresh rate (30)         
  --pix-format {mpeg|yuyv}  pixel format (mpeg)
)str";

template <class T>
auto parse(const std::string_view str) -> T {
    const auto o = from_chars<T>(str);
    if(!o) {
        print("not a number: ", str);
        exit(1);
    }
    return o.value();
}
} // namespace

auto parse_args(const int argc, const char* const argv[]) -> Args {
    auto args = Args();

    const auto increment = [argc](int& i) -> void {
        if(i + 1 >= argc) {
            printf("no following argument\n");
            exit(1);
        }
        i += 1;
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
        } else if(arg == "-d" || arg == "--device") {
            increment(i);
            args.video_device = argv[i];
        } else if(arg == "-s" || arg == "--server") {
            increment(i);
            args.event_fifo = argv[i];
        } else if(arg == "-o" || arg == "--output") {
            increment(i);
            args.savedir = argv[i];
        } else if(arg == "--width") {
            increment(i);
            args.width = parse<int>(argv[i]);
        } else if(arg == "--height") {
            increment(i);
            args.height = parse<int>(argv[i]);
        } else if(arg == "--fps") {
            increment(i);
            args.fps = parse<int>(argv[i]);
        } else if(arg == "--pix-format") {
            increment(i);
            if(const auto str = std::string_view(argv[i]); str == "mpeg") {
                args.pixel_format = PixelFormat::MPEG;
            } else if(str == "yuyv") {
                args.pixel_format = PixelFormat::YUYV;
            } else {
                print("unknown pixel format");
                exit(1);
            }
        }
    }

    return args;
}
