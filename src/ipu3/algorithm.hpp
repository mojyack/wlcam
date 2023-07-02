#pragma once
#include <utility>

namespace algo {
template <class T>
struct Size2 {
    T width;
    T height;
};

using Size = Size2<int>;

using FOV = Size2<double>;

struct PipeConfig {
    double bds_sf;
    Size   iif;
    Size   bds;
    Size   gdc;
};

auto calculate_pipeline_config(Size input, Size output, Size viewfinder) -> PipeConfig;

auto calculate_bds_grid(Size bds) -> std::pair<Size, Size>;
} // namespace algo
