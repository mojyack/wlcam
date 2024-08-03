#include <string_view>

#include "../args-parser.hpp"
#include "../macros/assert.hpp"
#include "../macros/unwrap.hpp"
#include "../util/charconv.hpp"
#include "../util/misc.hpp"
#include "args.hpp"

namespace args {
namespace {
using Params = ::ipu3::Args::Params;
template <>
auto from_string<Params>(const char* str) -> std::optional<Params> {
    auto elms = split(str, ",");
    auto ret  = Params(elms.size());
    for(auto i = 0u; i < elms.size(); i += 1) {
        auto pair = split(elms[i], "=");
        assert_o(pair.size() == 2);
        unwrap_oo(value, from_chars<int>(pair[1]));
        ret[i] = {elms[0], value};
    }
    return ret;
}

template <>
auto to_string<Params>(const Params& data) -> std::string {
    auto ret = std::string();
    for(const auto& param : data) {
        ret += param.first;
        ret += "=";
        ret += std::to_string(param.second);
        ret += ",";
    }
    ret.pop_back();
    return ret;
}
} // namespace
} // namespace args

namespace ipu3 {
auto Args::parse(const int argc, const char* const argv[]) -> std::optional<Args> {
    auto args   = Args();
    auto parser = args::Parser<Args::Params>();
    setup_common_args(args, parser);
    parser.kwarg(&args.cio2_devnode, {"--cio2"}, {"MEDIA_DEV", "device profile"});
    parser.kwarg(&args.imgu_devnode, {"--imgu"}, {"MEDIA_DEV", "device profile"});
    parser.kwarg(&args.cio2_entity, {"--cio2-entity"}, {"ENT_NAME", "device profile"});
    parser.kwarg(&args.imgu_entity, {"--imgu-entity"}, {"ENT_NAME", "device profile"});
    parser.kwarg(&args.sensor_mbus_code, {"--sensor-mbus-code"}, {"MBUS_CODE", "device profile"});
    parser.kwarg(&args.sensor_width, {"--sensor-width"}, {"WIDTH", "device profile"});
    parser.kwarg(&args.sensor_height, {"--sensor-height"}, {"HEIGHT", "device profile"});
    parser.kwarg(&args.ipu3_params, {"--params"}, {"KEY=VALUE,...", "ipu3 parameter, wb_gains.r, gamma, etc.", args::State::Initialized});
    if(!parser.parse(argc, argv) || args.help) {
        print("usage: wlcam-ipu3 ", parser.get_help());
        return std::nullopt;
    }
    return args;
}
} // namespace ipu3
