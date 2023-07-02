#pragma once
#include "../v4l2-wlctl/src/window.hpp"
#include "algorithm.hpp"
#include "intel-ipu3.h"

enum class ControlKind {
    WBGainR,
    WBGainB,
    WBGainGR,
    WBGainGB,
    BLCR,
    BLCB,
    BLCGR,
    BLCGB,
};

struct Control {
    std::string label;
    ControlKind kind;
    int         min;
    int         max;
    int         current;

    auto inactive() const -> bool {
        return false;
    }

    auto get_type() const -> vcw::ControlType {
        return vcw::ControlType::Int;
    }

    auto get_label() const -> std::string_view {
        return label;
    }

    auto get_max() const -> int {
        return max;
    }

    auto get_min() const -> int {
        return min;
    }

    auto get_step() const -> int {
        return 1;
    }

    auto get_current() const -> int {
        return current;
    }

    auto get_menu_size() const -> size_t {
        return 0;
    }

    auto get_menu_label(const int /*i*/) const -> std::string_view {
        return "";
    }

    auto get_menu_value(const int /*i*/) const -> int {
        return 0;
    }
};

using VCWindow = vcw::Window<Control>;

auto init_params_buffer(ipu3_uapi_params& params, const algo::PipeConfig& pipe_config, const ipu3_uapi_grid_config& bds_grid) -> void;

auto create_control_rows() -> std::vector<VCWindow::Row>;

auto apply_controls(ipu3_uapi_params** params_array, size_t params_array_size, Control& control, int value) -> void;
