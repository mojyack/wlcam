#include <algorithm>
#include <vector>

#include <stdio.h>

// IF: Input Feeder (former Decompressor)
// BDS: bayer downscaling
// GDC: Geometric Distortion Correction block
// DVS: Digital video stabilization
// SF: scale factor

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

constexpr auto FILTER_W = 4;
constexpr auto FILTER_H = 4;

constexpr auto IF_ALIGN_W  = 2;
constexpr auto IF_ALIGN_H  = 4;
constexpr auto BDS_ALIGN_W = 2;
constexpr auto BDS_ALIGN_H = 4;

constexpr auto IF_CROP_MAX_W = 40;
constexpr auto IF_CROP_MAX_H = 540;

constexpr auto BDS_SF_MAX  = 2.5;
constexpr auto BDS_SF_MIN  = 1;
constexpr auto BDS_SF_STEP = 1.0 / 32;

//constexpr auto YUV_SF_MAX  = 16;
//constexpr auto YUV_SF_MIN  = 1;
//constexpr auto YUV_SF_STEP = 1.0 / 4;

constexpr auto YUV_MAX_SCALE = 12;

auto is_diff_ratio(const Size a, const Size b) -> bool {
    return std::abs(1. * a.width / a.height - 1. * b.width / b.height) >= 0.1;
}

auto find_floor_value(const double min, const double max, const double step, const double value) -> double {
    if(value <= min) {
        return min;
    } else if(value >= max) {
        return max;
    }

    return min + int((value - min) / step) * step;
}

auto floor_aligned(const int step, const int value) -> int {
    return (value / step) * step;
}

auto ceil_aligned(const int step, const int value) -> int {
    return ((value + step - 1) / step) * step;
}

auto pipeconfigs = std::vector<PipeConfig>();

auto calculate_bds_height(const Size iif, const Size gdc, const int bds_width, const double bds_sf) -> void {
    const auto min_if_h  = iif.height - IF_CROP_MAX_H;
    const auto min_bds_h = gdc.height + FILTER_H * 2;
    auto       if_h      = 0;
    auto       bds_h     = 0.0;
    if(!is_diff_ratio(iif, gdc)) {
        auto found_if_h = 0;
        auto est_if_h   = 1. * iif.width * gdc.height / gdc.width;
        est_if_h        = std::clamp<double>(est_if_h, min_if_h, iif.height);

        auto func = [&](const int f) -> void {
            if_h = ceil_aligned(est_if_h, IF_ALIGN_H);
            while(if_h >= min_if_h && if_h <= iif.height && if_h / bds_sf >= min_bds_h) {
                const auto h = if_h / bds_sf;
                if(h - int(h) == 0) {
                    const auto bds_h_int = int(h);
                    if(bds_h_int % BDS_ALIGN_H == 0) {
                        found_if_h = if_h;
                        bds_h      = h;
                        break;
                    }
                }
                if_h += f * IF_ALIGN_H;
            }
        };

        func(1);
        func(-1);

        if(found_if_h != 0) {
            pipeconfigs.push_back({bds_sf, {iif.width, found_if_h}, {bds_width, int(bds_h)}, gdc});
        }
    } else {
        auto if_h = ceil_aligned(iif.height, IF_ALIGN_H);
        while(if_h >= min_if_h && if_h / bds_sf >= min_bds_h) {
            bds_h = if_h / bds_sf;
            if(if_h - int(if_h) == 0 && bds_h - int(bds_h) == 0) {
                if(if_h % IF_ALIGN_H == 0 && int(bds_h) % BDS_ALIGN_H == 0) {
                    pipeconfigs.push_back({bds_sf, {iif.width, if_h}, {bds_width, int(bds_h)}, gdc});
                }
            }
            if_h -= IF_ALIGN_H;
        }
    }
}

auto calculate_bds(const Size iif, const Size gdc, const double bds_sf) -> void {
    const auto min_bds_w = gdc.width + FILTER_W * 2;
    const auto min_bds_h = gdc.height + FILTER_H * 2;

    auto proc = [&](const int f) {
        auto sf = bds_sf;
        while(sf <= BDS_SF_MAX && sf >= BDS_SF_MIN) {
            const auto bds_w = iif.width / sf;
            const auto bds_h = iif.height / sf;
            if(bds_w - int(bds_w) == 0 && bds_h - int(bds_h) == 0) {
                if(int(bds_w) % BDS_ALIGN_W == 0 && bds_w >= min_bds_w &&
                   int(bds_h) % BDS_ALIGN_H == 0 && bds_h >= min_bds_h) {
                    calculate_bds_height(iif, gdc, bds_w, sf);
                }
            }
            sf += f * BDS_SF_STEP;
        }
    };

    proc(1);
    proc(-1);
}

auto calculate_gdc([[maybe_unused]] const Size input, const Size output, const Size viewfinder) -> Size {
    auto gdc_w = output.width;
    auto gdc_h = std::max(output.height, output.width * viewfinder.height / viewfinder.width);
    gdc_h      = std::min(gdc_h, viewfinder.height * YUV_MAX_SCALE);
    return {gdc_w, gdc_h};
}

auto calculate_fov(const Size input, const PipeConfig& config) -> FOV {
    auto in_w       = 1. * input.width;
    auto in_h       = 1. * input.height;
    auto if_crop_w  = in_w - config.iif.width;
    auto if_crop_h  = in_h - config.iif.height;
    auto gdc_crop_w = 1. * (config.bds.width - config.gdc.width) * config.bds_sf;
    auto gdc_crop_h = 1. * (config.bds.height - config.gdc.height) * config.bds_sf;
    return {
        (in_w - (if_crop_w + gdc_crop_w)) / in_w,
        (in_h - (if_crop_h + gdc_crop_h)) / in_h,
    };
}

auto main() -> int {
    const auto input      = Size{2592, 1944};
    const auto output     = Size{1280, 1080};
    const auto viewfinder = Size{1280, 780};

    if(input.width < IF_CROP_MAX_W || input.height < IF_CROP_MAX_H) {
        puts("not supported");
        return 1;
    }

    const auto gdc    = calculate_gdc(input, output, viewfinder);
    const auto bds_sf = 1. * input.width / gdc.width;
    const auto sf     = find_floor_value(BDS_SF_MIN, BDS_SF_MAX, BDS_SF_STEP, bds_sf);

    // height first
    auto min_if_w = input.width - IF_CROP_MAX_W;
    auto min_if_h = input.height - IF_CROP_MAX_H;
    auto if_w     = ceil_aligned(input.width, IF_ALIGN_W);
    auto if_h     = ceil_aligned(input.height, IF_ALIGN_H);
    while(if_w >= min_if_w) {
        while(if_h >= min_if_h) {
            auto iif = Size{if_w, if_h};
            calculate_bds(iif, gdc, sf);
            if_h -= IF_ALIGN_H;
        }
        if_w -= IF_ALIGN_W;
    }

    // width first
    min_if_w = input.width - IF_CROP_MAX_W;
    min_if_h = input.height - IF_CROP_MAX_H;
    if_w     = ceil_aligned(input.width, IF_ALIGN_W);
    if_h     = ceil_aligned(input.height, IF_ALIGN_H);
    while(if_h >= min_if_h) {
        while(if_w >= min_if_w) {
            auto iif = Size{if_w, if_h};
            calculate_bds(iif, gdc, sf);
            if_w -= IF_ALIGN_W;
        }
        if_h -= IF_ALIGN_H;
    }

    if(pipeconfigs.empty()) {
        puts("no configs found");
        return 1;
    }

    auto best_fov   = calculate_fov(input, pipeconfigs[0]);
    auto best_index = 0u;
    for(auto i = 1u; i < pipeconfigs.size(); i += 1) {
        auto fov = calculate_fov(input, pipeconfigs[i]);
        if(fov.width >= best_fov.width || (fov.width == best_fov.width && fov.height >= fov.height)) {
            best_fov   = fov;
            best_index = i;
        }
    }

    const auto& conf = pipeconfigs[best_index];
    printf("IF:  %u,%u\n", conf.iif.width, conf.iif.height);
    printf("BDS: %u,%u\n", conf.bds.width, conf.bds.height);
    printf("GDC: %u,%u\n", conf.gdc.width, conf.gdc.height);
    printf("FOV: %f,%f\n", best_fov.width, best_fov.height);

    return 0;
}
