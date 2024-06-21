// generic planar
// YYYY.... U(U...).... V(V...)....
// ppc: pixel per chrominance

#pragma once
#include "multitex.hpp"

auto init_planar_shader() -> void;

class PlanarGraphic : public MultiTex {
  public:
    auto update_texture(int width, int height, int stride, int ppc_x, int ppc_y, const std::byte* y, const std::byte* u, const std::byte* v) -> void;

    PlanarGraphic();
};
