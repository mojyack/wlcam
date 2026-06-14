// MIPI RAW10 packed Bayer (V4L2_PIX_FMT_S*10P)
// ported from libcamera's bayer_1x_packed.frag. (BSD-2, Morgan McGuire / Linaro).

#pragma once
#include <array>

#include "../gawl/graphic-base.hpp"

// Bayer order, expressed as the position of the first red pixel.
// SRGGB -> (0,0), SGRBG -> (1,0), SGBRG -> (0,1), SBGGR -> (1,1).
struct BayerParams {
    std::array<float, 2> first_red   = {0.0f, 0.0f};          // SRGGB10P
    float                black_level = 16.0f / 255.0f;        // 10-bit 64 -> 8-bit MSB
    std::array<float, 3> wb_gain     = {1.70f, 1.07f, 1.60f}; // R,G,B gain (incl. normalize)
    float                gamma       = 1.0f / 2.2f;           // shader applies pow(rgb, gamma)
    int                  cell        = 1;                     // 1=standard bayer, 2=quad bayer (2x2 binned)
    int                  rotate      = 0;                     // clockwise output rotation: 0/1/2/3 = 0/90/180/270 deg
    float                lsc         = 0.0f;                  // lens shading correction: 0=off, radial gain at corner = 1+strength
};

inline auto bayer_params = BayerParams();

auto init_bayer_shader() -> bool;

class BayerGraphic : public gawl::impl::GraphicBase {
  public:
    auto update_texture(int width, int height, int stride, const std::byte* data) -> void;

    BayerGraphic();
};
