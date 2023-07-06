#include "params.hpp"
#include "uapi.hpp"

namespace {
template <class T>
auto cdiv(const T a, const T b) -> T {
    return (a + b - 1) / b;
}

auto apply_gamma_lut(ipu3_uapi_params& params, const double gamma) -> void {
    for(auto i = 0u; i < IPU3_UAPI_GAMMA_CORR_LUT_ENTRIES; i += 1) {
        auto j = 1. * i / (IPU3_UAPI_GAMMA_CORR_LUT_ENTRIES - 1);
        auto g = std::pow(j, 1.0 / gamma);

        params.acc_param.gamma.gc_lut.lut[i] = g * 8191;
    }
}

auto is_valid_shd_grid_size(const int num_grids_x, const int num_grids_y) -> bool {
    if(num_grids_x > IPU3_UAPI_SHD_MAX_CELLS_PER_SET) {
        return false;
    }
    const auto slices_per_set = IPU3_UAPI_SHD_MAX_CELLS_PER_SET / num_grids_x;
    if(cdiv(num_grids_y, slices_per_set) > IPU3_UAPI_SHD_MAX_CFG_SETS) {
        return false;
    }
    return true;
}

auto apply_shd_lut(ipu3_uapi_params& params, const double gain) -> void {
    auto&      shd          = params.acc_param.shd.shd;
    auto&      grid         = shd.grid;
    auto&      lut          = params.acc_param.shd.shd_lut;
    const auto center_x     = (grid.width - 1) / 2.0;
    const auto center_y     = (grid.height - 1) / 2.0;
    const auto max_distance = std::pow(center_x, 2) + std::pow(center_y, 2);
    const auto sets         = cdiv(grid.height, grid.grid_height_per_slice);
    for(auto s = 0; s < sets; s += 1) {
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
        auto block_width_log2  = 4;
        auto block_height_log2 = 4;
        auto block_width       = 1 << block_width_log2;
        auto block_height      = 1 << block_height_log2;
        while(true) {
            if(is_valid_shd_grid_size(cdiv(pipe_config.bds.width, block_width), cdiv(pipe_config.bds.height, block_height))) {
                break;
            }
            block_width_log2 += 1;
            block_width <<= 1;
            if(is_valid_shd_grid_size(cdiv(pipe_config.bds.width, block_width), cdiv(pipe_config.bds.height, block_height_log2))) {
                break;
            }
            block_height_log2 += 1;
            block_height <<= 1;
        }

        auto& shd         = params.acc_param.shd.shd;
        auto& grid        = shd.grid;
        auto& general     = shd.general;
        auto& black_level = shd.black_level;

        grid.width                    = cdiv(pipe_config.bds.width, block_width);
        grid.height                   = cdiv(pipe_config.bds.height, block_height);
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
        // from ipu3-tables.c
        static const auto defaults = ipu3_uapi_anr_config{
            .transform = {
                .enable                = 1,
                .adaptive_treshhold_en = 1,
                .alpha                 = {{13, 13, 13, 13, 0, 0, 0, 0},
                                          {11, 11, 11, 11, 0, 0, 0, 0},
                                          {14, 14, 14, 14, 0, 0, 0, 0}},
                .beta                  = {{24, 24, 24, 24},
                                          {21, 20, 20, 21},
                                          {25, 25, 25, 25}},
                .color                 = {{{166, 173, 149, 166, 161, 146, 145, 173, 145, 150, 141, 149, 145, 141, 142},
                                           {166, 173, 149, 165, 161, 145, 145, 173, 145, 150, 141, 149, 145, 141, 142},
                                           {166, 174, 149, 166, 162, 146, 146, 173, 145, 150, 141, 149, 145, 141, 142},
                                           {166, 173, 149, 165, 161, 145, 145, 173, 146, 150, 141, 149, 145, 141, 142}},
                                          {{141, 131, 140, 141, 144, 143, 144, 131, 143, 137, 140, 140, 144, 140, 141},
                                           {141, 131, 140, 141, 143, 143, 144, 131, 143, 137, 140, 140, 144, 140, 141},
                                           {141, 131, 141, 141, 144, 144, 144, 131, 143, 137, 140, 140, 144, 140, 141},
                                           {140, 131, 140, 141, 143, 143, 144, 131, 143, 137, 140, 140, 144, 140, 141}},
                                          {{184, 173, 188, 184, 182, 182, 181, 173, 182, 179, 182, 188, 181, 182, 180},
                                           {184, 173, 188, 184, 183, 182, 181, 173, 182, 178, 182, 188, 181, 182, 180},
                                           {184, 173, 188, 184, 182, 182, 181, 173, 182, 178, 182, 188, 181, 182, 181},
                                           {184, 172, 188, 184, 182, 182, 181, 173, 182, 178, 182, 188, 182, 182, 180}}},
                .sqrt_lut              = {724, 768, 810, 849, 887, 923, 958, 991, 1024,
                                          1056, 1086, 1116, 1145, 1173, 1201, 1228, 1254,
                                          1280, 1305, 1330, 1355, 1379, 1402, 1425, 1448},
                .xreset                = -1632,
                .yreset                = -1224,
                .x_sqr_reset           = 2663424,
                .r_normfactor          = 14,
                .y_sqr_reset           = 1498176,
                .gain_scale            = 115},
            .stitch = {
                .anr_stitch_en = 1,
                .pyramid       = {
                    {1, 3, 5, {}},
                    {7, 7, 5, {}},
                    {3, 1, 3, {}},
                    {9, 15, 21, {}},
                    {21, 15, 9, {}},
                    {3, 5, 15, {}},
                    {25, 35, 35, {}},
                    {25, 15, 5, {}},
                    {7, 21, 35, {}},
                    {49, 49, 35, {}},
                    {21, 7, 7, {}},
                    {21, 35, 49, {}},
                    {49, 35, 21, {}},
                    {7, 5, 15, {}},
                    {25, 35, 35, {}},
                    {25, 15, 5, {}},
                    {3, 9, 15, {}},
                    {21, 21, 15, {}},
                    {9, 3, 1, {}},
                    {3, 5, 7, {}},
                    {7, 5, 3, {}},
                    {1, {}, {}, {}},
                },
            },
        };

        params.acc_param.anr = defaults;
        // params.use.acc_anr   = 1;
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

        struct ipu3_uapi_isp_xnr3_vmem_params_ {
            int16_t x[16];
            int16_t reserved0[IPU3_UAPI_ISP_VEC_ELEMS - 16];
            int16_t a[16];
            int16_t reserved1[IPU3_UAPI_ISP_VEC_ELEMS - 16];
            int16_t b[16];
            int16_t reserved2[IPU3_UAPI_ISP_VEC_ELEMS - 16];
            int16_t c[16];
            int16_t reserved3[IPU3_UAPI_ISP_VEC_ELEMS - 16];
        } __attribute__((packed));

        static const auto defaults = ipu3_uapi_isp_xnr3_vmem_params_{
            .x = {1024, 1164, 1320, 1492, 1680, 1884, 2108, 2352, 2616, 2900, 3208, 3540, 3896, 4276, 4684, 5120},
            .a = {-7213, -5580, -4371, -3421, -2722, -2159, -6950, -5585, -4529, -3697, -3010, -2485, -2070, -1727, -1428, 0},
            .b = {4096, 3603, 3178, 2811, 2497, 2226, 1990, 1783, 1603, 1446, 1307, 1185, 1077, 981, 895, 819},
            .c = {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        };

        memcpy(&params.xnr3_vmem_params, &defaults, sizeof(ipu3_uapi_isp_xnr3_vmem_params));
        // params.use.xnr3_vmem_params = 1;
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
        case ControlKind::User1:
        case ControlKind::User2:
        case ControlKind::User3:
        case ControlKind::User4:
        case ControlKind::User5:
            break;
        }
    }
}
