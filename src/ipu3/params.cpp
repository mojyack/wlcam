#include "params.hpp"
#include "uapi.hpp"

auto init_params_buffer(ipu3_uapi_params& params, const algo::PipeConfig& pipe_config, const ipu3_uapi_grid_config& bds_grid) -> void {
    params.acc_param.awb.config.rgbs_thr_r  = 8191.0;
    params.acc_param.awb.config.rgbs_thr_gr = 8191.0;
    params.acc_param.awb.config.rgbs_thr_gb = 8191.0;
    params.acc_param.awb.config.rgbs_thr_b  = 8191.0;
    params.acc_param.awb.config.rgbs_thr_b |= IPU3_UAPI_AWB_RGBS_THR_B_INCL_SAT | IPU3_UAPI_AWB_RGBS_THR_B_EN;

    params.acc_param.awb.config.grid = bds_grid;

    params.acc_param.bnr                            = uapi::css_bnr_defaults;
    params.acc_param.bnr.column_size                = pipe_config.bds.width;
    const auto x_reset                              = bds_grid.x_start - (pipe_config.bds.width / 2);
    const auto y_reset                              = bds_grid.y_start - (pipe_config.bds.height / 2);
    params.acc_param.bnr.opt_center.x_reset         = x_reset;
    params.acc_param.bnr.opt_center.y_reset         = y_reset;
    params.acc_param.bnr.opt_center_sqr.x_sqr_reset = x_reset * x_reset;
    params.acc_param.bnr.opt_center_sqr.y_sqr_reset = y_reset * y_reset;

    params.acc_param.bnr.wb_gains.r  = 0xFFFF;
    params.acc_param.bnr.wb_gains.gr = 0;
    params.acc_param.bnr.wb_gains.gb = 0;
    params.acc_param.bnr.wb_gains.b  = 0;
    params.acc_param.ccm             = uapi::css_ccm_defaults;

    params.use.acc_awb = 1;
    params.use.acc_bnr = 1;
    params.use.acc_ccm = 1;
}

auto create_control_rows() -> std::vector<VCWindow::Row> {
    auto ret = std::vector<VCWindow::Row>();
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.r", ControlKind::WBGainR, 0, 0xFFFF, 0});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.b", ControlKind::WBGainB, 0, 0xFFFF, 0});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.gr", ControlKind::WBGainGR, 0, 0xFFFF, 0});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.gb", ControlKind::WBGainGB, 0, 0xFFFF, 0});
    return ret;
}

auto apply_controls(ipu3_uapi_params** const params_array, const size_t params_array_size, Control& control, const int value) -> void {
    control.current = value;
    for(auto i = 0u; i < params_array_size; i += 1) {
        auto& params = *params_array[i];
        switch(control.kind) {
        case ControlKind::WBGainR:
            params.acc_param.bnr.wb_gains.r = value;
            break;
        case ControlKind::WBGainB:
            params.acc_param.bnr.wb_gains.b = value;
            break;
        case ControlKind::WBGainGR:
            params.acc_param.bnr.wb_gains.gr = value;
            break;
        case ControlKind::WBGainGB:
            params.acc_param.bnr.wb_gains.gb = value;
            break;
        }
    }
}
