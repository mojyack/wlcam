#include <string_view>

#include "../macros/assert.hpp"
#include "../util/assert.hpp"
#include "../util/charconv.hpp"
#include "args.hpp"

namespace {
const auto help = R"str(usage: wlcam-ipu3 [FLAG|OPTION]...
flags:
  -h, --help                   print this message
options:
  -s, --server PATH            create fifo for companion
  -o, --output PATH            output directory
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
    DYN_ASSERT(o.has_value(), "not a number: ", str);
    return o.value();
}
} // namespace

namespace ipu3 {
auto parse_args(const int argc, const char* const argv[]) -> Args {
    auto args = Args();

    const auto increment = [argc](int& i) -> void {
        i += 1;
        DYN_ASSERT(i < argc, "no following argument");
    };

    for(auto i = 1; i < argc; i += 1) {
        const auto arg = std::string_view(argv[i]);
        if(arg == "-h" || arg == "--help") {
            print(help);
            exit(0);
        } else if(arg == "-s" || arg == "--server") {
            increment(i);
            args.event_fifo = argv[i];
        } else if(arg == "-o" || arg == "--output") {
            increment(i);
            args.savedir = argv[i];
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

    DYN_ASSERT(args.savedir != nullptr, "no --output argument");
    DYN_ASSERT(args.cio2_devnode != nullptr, "no --cio2 argument");
    DYN_ASSERT(args.imgu_devnode != nullptr, "no --imgu argument");
    DYN_ASSERT(args.cio2_entity != nullptr, "no --cio2-entity argument");
    DYN_ASSERT(args.imgu_entity != nullptr, "no --imgu-entity argument");
    DYN_ASSERT(args.sensor_mbus_code != 0, "no --sensor-mbus-code argument");
    DYN_ASSERT(args.sensor_width != 0, "no --sensor-width argument");
    DYN_ASSERT(args.sensor_height != 0, "no --sensor-height argument");
    DYN_ASSERT(args.width != 0, "no --width argument");
    DYN_ASSERT(args.height != 0, "no --height argument");

    return args;
}

} // namespace ipu3
