#pragma once
#include <optional>

#include "../args.hpp"

namespace camss {
struct Args : CommonArgs {
    const char* media_device = "/dev/media0";
    uint8_t     csiphy;
    uint8_t     csid        = 0;
    uint8_t     vfe         = 0;
    uint8_t     cell        = 1;
    uint16_t    rotate      = 0; // clockwise rotation of the image: 0/90/180/270
    uint16_t    gamma       = 1.0 / 2.2 * 100;
    uint16_t    black_level = 16.0 / 255.0 * 255;
    uint16_t    wb_r        = 1.70 * 100;
    uint16_t    wb_g        = 1.07 * 100;
    uint16_t    wb_b        = 1.60 * 100;
    uint16_t    lsc         = 0.5 * 100;

    static auto parse(int argc, const char* const* argv) -> std::optional<Args>;
};
} // namespace camss
