#pragma once
#include <memory>

#include "../ui.hpp"
#include "../util/light-map.hpp"
#include "algorithm.hpp"
#include "intel-ipu3.h"

inline auto params_buffers = std::span<ipu3_uapi_params*>();

auto init_params_buffer(ipu3_uapi_params& params, const algo::PipeConfig& pipe_config, const ipu3_uapi_grid_config& bds_grid) -> void;
auto create_buttons(std::vector<std::unique_ptr<Button>>& buttons, const LightMap<std::string_view, int>& inits) -> void;
