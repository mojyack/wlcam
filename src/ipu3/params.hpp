#pragma once
#include "../vcw/window.hpp"
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
    GammaCollection,
    LensShading,
    User1,
    User2,
    User3,
    User4,
    User5,
};

struct Control : vcw::Control {
    std::string label;
    ControlKind kind;
    int         min;
    int         max;
    int         current;

    auto is_active() -> bool override;
    auto get_type() -> vcw::ControlType override;
    auto get_label() -> std::string_view override;
    auto get_range() -> vcw::ValueRange override;
    auto get_current() -> int override;
    auto get_menu_size() -> size_t override;
    auto get_menu_label(size_t index) -> std::string_view override;
    auto get_menu_value(size_t index) -> int override;

    Control(std::string label, ControlKind kind, int min, int max, int current);
};

struct ParamsCallbacks : public vcw::UserCallbacks {
    ipu3_uapi_params** params_array;
    size_t             params_array_size;

    auto set_control_value(vcw::Control& control, int value) -> void override;
    // TODO
    auto quit() -> void override {
        std::quick_exit(0);
    }
};

auto init_params_buffer(ipu3_uapi_params& params, const algo::PipeConfig& pipe_config, const ipu3_uapi_grid_config& bds_grid) -> void;
auto create_control_rows() -> std::vector<vcw::Row>;
auto apply_controls(ipu3_uapi_params** params_array, size_t params_array_size, Control& control, int value) -> void;
