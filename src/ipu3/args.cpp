#include <string_view>

#include "../util/charconv.hpp"
#include "../util/error.hpp"
#include "args.hpp"

namespace {
const auto help = R"str(usage: wlcam-ipu3 [FLAG|OPTION]...
flags:
  -h, --help                   print this message
options:
  --cio2 MEDIA_DEV             (device profile)  
  --imgu MEDIA_DEV             (device profile)
  --cio2-entity ENT_NAME       (device profile)
  --imgu-entity ENT_NAME       (device profile)
  --sensor-mbus-code MBUS_CODE (device profile)
  --sensor-width WIDTH         (device profile)
  --sensor-height HEIGHT       (device profile)
  --width  WIDTH               horizontal resolution
  --height HEIGHT vertical     resolution
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

namespace ipu3 {
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
        } else if(arg == "--cio2") {
            increment(i);
            args.cio2_devnode = argv[i];
        } else if(arg == "--imgu") {
            increment(i);
            args.imgu_devnode = argv[i];
        } else if(arg == "--cio2-entity") {
            increment(i);
            args.cio2_entity = argv[i];
        } else if(arg == "--imgu-entity") {
            increment(i);
            args.imgu_entity = argv[i];
        } else if(arg == "--sensor-mbus-code") {
            increment(i);
            args.sensor_mbus_code = parse<int>(argv[i]);
        } else if(arg == "--sensor-width") {
            increment(i);
            args.sensor_width = parse<int>(argv[i]);
        } else if(arg == "--sensor-height") {
            increment(i);
            args.sensor_height = parse<int>(argv[i]);
        } else if(arg == "--width") {
            increment(i);
            args.width = parse<int>(argv[i]);
        } else if(arg == "--height") {
            increment(i);
            args.height = parse<int>(argv[i]);
        }
    }

    if(args.cio2_devnode.empty()) {
        print("no --cio2 argument");
        exit(0);
    }
    if(args.imgu_devnode.empty()) {
        print("no --imgu argument");
        exit(0);
    }
    if(args.cio2_entity.empty()) {
        print("no --cio2-entity argument");
        exit(0);
    }
    if(args.imgu_entity.empty()) {
        print("no --imgu-entity argument");
        exit(0);
    }
    if(args.sensor_mbus_code == 0) {
        print("no --sensor-mbus-code argument");
        exit(0);
    }
    if(args.sensor_width == 0) {
        print("no --sensor-width argument");
        exit(0);
    }
    if(args.sensor_height == 0) {
        print("no --sensor-height argument");
        exit(0);
    }
    if(args.width == 0) {
        print("no --width argument");
        exit(0);
    }
    if(args.height == 0) {
        print("no --height argument");
        exit(0);
    }
    return args;
}

} // namespace ipu3
