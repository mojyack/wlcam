#include "params.hpp"
#include "uapi.hpp"

namespace {
auto apply_gamma_lut(ipu3_uapi_params& params, const double gamma) -> void {
    for(auto i = 0u; i < IPU3_UAPI_GAMMA_CORR_LUT_ENTRIES; i += 1) {
        auto j = 1. * i / (IPU3_UAPI_GAMMA_CORR_LUT_ENTRIES - 1);
        auto g = std::pow(j, 1.0 / gamma);

        params.acc_param.gamma.gc_lut.lut[i] = g * 8191;
    }
}

auto apply_shd_lut(ipu3_uapi_params& params, const double gain) -> void {
    auto&      shd          = params.acc_param.shd.shd;
    auto&      grid         = shd.grid;
    auto&      lut          = params.acc_param.shd.shd_lut;
    const auto center_x     = (grid.width - 1) / 2.0;
    const auto center_y     = (grid.height - 1) / 2.0;
    const auto max_distance = std::pow(center_x, 2) + std::pow(center_y, 2);
    for(auto s = 0u; s < grid.height / grid.grid_height_per_slice; s += 1) {
        for(auto r_ = 0u; r_ < grid.grid_height_per_slice; r_ += 1) {
            const auto r = s * grid.grid_height_per_slice + r_;
            for(auto c = 0u; c < grid.width; c += 1) {
                const auto distance                          = (std::pow(center_x - c, 2) + std::pow(center_y - r, 2)) / max_distance;
                lut.sets[s].r_and_gr[r_ * grid.width + c].r  = 8192 * distance * 0.19 * gain;
                lut.sets[s].r_and_gr[r_ * grid.width + c].gr = 8192 * distance * 0.16 * gain;
                lut.sets[s].gb_and_b[r_ * grid.width + c].gb = 8192 * distance * 0.16 * gain;
                lut.sets[s].gb_and_b[r_ * grid.width + c].b  = 8192 * distance * 0.17 * gain;
            }
        }
    }
}
} // namespace

auto init_params_buffer(ipu3_uapi_params& params, const algo::PipeConfig& pipe_config, const ipu3_uapi_grid_config& bds_grid) -> void {
    // https://docs.kernel.org/admin-guide/media/ipu3.html

    // Optical Black Correction
    {
        params.obgrid_param.r   = 64;
        params.obgrid_param.b   = 64;
        params.obgrid_param.gr  = 64;
        params.obgrid_param.gb  = 64;
        params.use.obgrid       = 1;
        params.use.obgrid_param = 1;
    }

    // Linearization
    {
        // params.lin_vmem_params;
    }

    // SHD(Lens shading correction)
    {
        constexpr auto block_width_log2  = 6; // 32px
        constexpr auto block_height_log2 = 6; // 32px
        constexpr auto block_width       = 1 << block_width_log2;
        constexpr auto block_height      = 1 << block_height_log2;

        const auto num_grids_x = (pipe_config.bds.width + block_width - 1) / block_width;
        const auto num_grids_y = (pipe_config.bds.height + block_height - 1) / block_height;

        auto& shd                     = params.acc_param.shd.shd;
        auto& grid                    = shd.grid;
        auto& general                 = shd.general;
        auto& black_level             = shd.black_level;
        grid.width                    = num_grids_x;
        grid.height                   = num_grids_y;
        grid.block_width_log2         = block_width_log2;
        grid.block_height_log2        = block_height_log2;
        grid.grid_height_per_slice    = IPU3_UAPI_SHD_MAX_CELLS_PER_SET / grid.width;
        grid.x_start                  = 0;
        grid.y_start                  = 0;
        general.init_set_vrt_offst_ul = grid.y_start >> grid.block_height_log2 % grid.grid_height_per_slice;
        general.shd_enable            = 1;
        general.gain_factor           = 1;
        black_level.bl_r              = 0;
        black_level.bl_gr             = 0;
        black_level.bl_gr             = 0;
        black_level.bl_b              = 0;

        if(num_grids_x > IPU3_UAPI_SHD_MAX_CELLS_PER_SET || num_grids_y / grid.grid_height_per_slice > IPU3_UAPI_SHD_MAX_CFG_SETS) {
            printf("input too large to enable shd\n");
            printf("num_grids_x: %u >? %u\n", num_grids_x, IPU3_UAPI_SHD_MAX_CELLS_PER_SET);
            printf("num_grids_y / grid.grid_height_per_slice: %u >? %u\n", num_grids_y / grid.grid_height_per_slice, IPU3_UAPI_SHD_MAX_CFG_SETS);
            exit(1);
        }

        apply_shd_lut(params, 1);
        params.use.acc_shd = 1;
    }

    // White Balance / Exposure / Focus Apply
    {

        params.acc_param.awb.config.rgbs_thr_r  = 8191.0;
        params.acc_param.awb.config.rgbs_thr_gr = 8191.0;
        params.acc_param.awb.config.rgbs_thr_gb = 8191.0;
        params.acc_param.awb.config.rgbs_thr_b  = 8191.0;
        params.acc_param.awb.config.rgbs_thr_b |= IPU3_UAPI_AWB_RGBS_THR_B_INCL_SAT | IPU3_UAPI_AWB_RGBS_THR_B_EN;
        params.acc_param.awb.config.grid = bds_grid;

        params.use.acc_awb = 1;
    }

    // BNR(Bayer noise reduction)
    {
        params.acc_param.bnr                            = uapi::css_bnr_defaults;
        params.acc_param.bnr.column_size                = pipe_config.bds.width;
        const auto x_reset                              = bds_grid.x_start - (pipe_config.bds.width / 2);
        const auto y_reset                              = bds_grid.y_start - (pipe_config.bds.height / 2);
        params.acc_param.bnr.opt_center.x_reset         = x_reset;
        params.acc_param.bnr.opt_center.y_reset         = y_reset;
        params.acc_param.bnr.opt_center_sqr.x_sqr_reset = x_reset * x_reset;
        params.acc_param.bnr.opt_center_sqr.y_sqr_reset = y_reset * y_reset;
        params.use.acc_bnr                              = 1;
    }

    // ANR(Advanced Noise Reduction)
    {
        // params.acc_param.anr;
    }

    // DM(Demosaicing converts)
    {
        // params.acc_param.dm;
    }

    // Color Correction
    {
        params.acc_param.ccm = uapi::css_ccm_defaults;
        params.use.acc_ccm   = 1;
    }

    // Gamma correction
    {
        apply_gamma_lut(params, 1 + 16 / 128.0);
        params.use.acc_gamma                  = 1;
        params.acc_param.gamma.gc_ctrl.enable = 1;
    }

    // CSC(Color space conversion)
    {
        // params.acc_param.csc;
    }

    // CDS(Chroma down sampling)
    {
        // params.acc_param.cds;
    }

    // CHNR(Chroma noise reduction)
    {
        // params.acc_param.chnr;
    }

    // TCC(Total color correction)
    {
        // params.acc_param.tcc;
    }

    // XNR3(eXtreme Noise Reduction V3)
    {
        // params.xnr3_vmem_params;
        // params.xnr3_dmem_params;
    }

    // TNR(Temporal Noise Reduction)
    {
        // params.tnr3_vmem_params;
        // params.tnr3_dmem_params;
    }
}

auto create_control_rows() -> std::vector<VCWindow::Row> {
    auto ret = std::vector<VCWindow::Row>();
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.r", ControlKind::WBGainR, 0, 0xFFFF, 16});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.b", ControlKind::WBGainB, 0, 0xFFFF, 16});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.gr", ControlKind::WBGainGR, 0, 0xFFFF, 16});
    ret.emplace_back(vcw::Tag<Control>(), Control{"wb_gains.gb", ControlKind::WBGainGB, 0, 0xFFFF, 16});
    ret.emplace_back(vcw::Tag<Control>(), Control{"obgrid_param.r", ControlKind::WBGainR, 0, 0xFFFF, 64});
    ret.emplace_back(vcw::Tag<Control>(), Control{"obgrid_param.b", ControlKind::WBGainB, 0, 0xFFFF, 64});
    ret.emplace_back(vcw::Tag<Control>(), Control{"obgrid_param.gr", ControlKind::WBGainGR, 0, 0xFFFF, 64});
    ret.emplace_back(vcw::Tag<Control>(), Control{"obgrid_param.gb", ControlKind::WBGainGB, 0, 0xFFFF, 64});
    ret.emplace_back(vcw::Tag<Control>(), Control{"gamma", ControlKind::GammaCollection, 0, 512, 16});
    ret.emplace_back(vcw::Tag<Control>(), Control{"lens shading", ControlKind::LensShading, -128, 128, 0});

    ret.emplace_back(vcw::Tag<vcw::Label<vcw::LabelType::Quit>>(), vcw::Label<vcw::LabelType::Quit>{0, "Quit"});

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
        case ControlKind::BLCR:
            params.obgrid_param.r = value;
            break;
        case ControlKind::BLCB:
            params.obgrid_param.b = value;
            break;
        case ControlKind::BLCGR:
            params.obgrid_param.gr = value;
            break;
        case ControlKind::BLCGB:
            params.obgrid_param.gb = value;
            break;
        case ControlKind::GammaCollection:
            apply_gamma_lut(params, 1 + value / 128.0);
            break;
        case ControlKind::LensShading:
            apply_shd_lut(params, 1 + value / 128.0);
            break;
        }
    }
}
