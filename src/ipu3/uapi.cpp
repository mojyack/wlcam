#include "uapi.hpp"

namespace uapi {
const ipu3_uapi_bnr_static_config css_bnr_defaults = {
    .wb_gains     = {16, 16, 16, 16},
    .wb_gains_thr = {255, 255, 255, 255},
    .thr_coeffs   = {1700, 0, 31, 31, 0, 16},
    .thr_ctrl_shd = {26, 26, 26, 26},
    .opt_center   = {-648, 0, -366, 0},
    .lut          = {
        {17, 23, 28, 32, 36, 39, 42, 45,
                  48, 51, 53, 55, 58, 60, 62, 64,
                  66, 68, 70, 72, 73, 75, 77, 78,
                  80, 82, 83, 85, 86, 88, 89, 90}},
    .bp_ctrl        = {20, 0, 1, 40, 0, 6, 0, 6, 0},
    .dn_detect_ctrl = {9, 3, 4, 0, 8, 0, 1, 1, 1, 1, 0},
    .column_size    = 1296,
    .opt_center_sqr = {419904, 133956},
};

const ipu3_uapi_ccm_mat_config css_ccm_defaults = {
    8191, 0, 0, 0,
    0, 8191, 0, 0,
    0, 0, 8191, 0};

auto create_bds_grid(const algo::Size bds) -> ipu3_uapi_grid_config {
    const auto [best, best_log2] = algo::calculate_bds_grid(bds);

    auto bds_grid = ipu3_uapi_grid_config();

    bds_grid.x_start           = 0;
    bds_grid.y_start           = 0;
    bds_grid.width             = best.width >> best_log2.width;
    bds_grid.block_width_log2  = best_log2.width;
    bds_grid.height            = best.height >> best_log2.height;
    bds_grid.block_height_log2 = best_log2.height;

    return bds_grid;
}
} // namespace uapi
