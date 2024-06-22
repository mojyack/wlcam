#include <string_view>

#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "../util/charconv.hpp"
#include "args.hpp"

namespace {
const auto help = R"str(usage: wlcam-ipu3 [FLAG|OPTION]...
flags:
  -h, --help                   print this message
options:
  -o, --output PATH            output directory
  --cio2 MEDIA_DEV             (device profile)  
  --imgu MEDIA_DEV             (device profile)
  --cio2-entity ENT_NAME       (device profile)
  --imgu-entity ENT_NAME       (device profile)
  --sensor-mbus-code MBUS_CODE (device profile)
  --sensor-width WIDTH         (device profile)
  --sensor-height HEIGHT       (device profile)
  --width  WIDTH               horizontal resolution
  --height HEIGHT              vertical resolution
  --param KEY VALUE            ipu3 parameter, wb_gains.r, gamma, etc.
                               this argument can be specified multiple times.
)str";
} // namespace

namespace ipu3 {
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
        } else if(arg == "-o" || arg == "--output") {
            assert_o(increment(i));
            args.savedir = argv[i];
        } else if(arg == "--cio2") {
            assert_o(increment(i));
            args.cio2_devnode = argv[i];
        } else if(arg == "--imgu") {
            assert_o(increment(i));
            args.imgu_devnode = argv[i];
        } else if(arg == "--cio2-entity") {
            assert_o(increment(i));
            args.cio2_entity = argv[i];
        } else if(arg == "--imgu-entity") {
            assert_o(increment(i));
            args.imgu_entity = argv[i];
        } else if(arg == "--sensor-mbus-code") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.sensor_mbus_code = num;
        } else if(arg == "--sensor-width") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.sensor_width = num;
        } else if(arg == "--sensor-height") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.sensor_height = num;
        } else if(arg == "--width") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.width = num;
        } else if(arg == "--height") {
            assert_o(increment(i));
            unwrap_oo(num, from_chars<int>(argv[i]));
            args.height = num;
        } else if(arg == "--param") {
            assert_o(increment(i));
            const auto key = argv[i];
            assert_o(increment(i));
            unwrap_oo(value, from_chars<int>(argv[i]));
            args.ipu3_params.emplace_back(key, value);
        }
    }

    assert_o(args.savedir != nullptr, "no --output argument");
    assert_o(args.cio2_devnode != nullptr, "no --cio2 argument");
    assert_o(args.imgu_devnode != nullptr, "no --imgu argument");
    assert_o(args.cio2_entity != nullptr, "no --cio2-entity argument");
    assert_o(args.imgu_entity != nullptr, "no --imgu-entity argument");
    assert_o(args.sensor_mbus_code != 0, "no --sensor-mbus-code argument");
    assert_o(args.sensor_width != 0, "no --sensor-width argument");
    assert_o(args.sensor_height != 0, "no --sensor-height argument");
    assert_o(args.width != 0, "no --width argument");
    assert_o(args.height != 0, "no --height argument");

    return args;
}

} // namespace ipu3
