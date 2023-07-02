#pragma once
#include "intel-ipu3.h"
#include "algorithm.hpp"

namespace uapi {
extern const ipu3_uapi_bnr_static_config css_bnr_defaults;
extern const ipu3_uapi_ccm_mat_config    css_ccm_defaults;

auto create_bds_grid(algo::Size bds) -> ipu3_uapi_grid_config;
} // namespace uapi
