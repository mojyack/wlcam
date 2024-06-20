#include <array>
#include <vector>

#include <stdio.h>
#include <turbojpeg.h>

#include "jpeg.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"

declare_autoptr(TJHandle, void, tjDestroy);

namespace {
// pixel per chrominance
auto tjsample_to_ppc(const int sample) -> std::optional<std::array<int, 2>> {
    switch(sample) {
    case TJSAMP_422:
        return std::array{2, 1};
    case TJSAMP_420:
        return std::array{2, 2};
    default:
        WARN("unsupported sampling type ", sample);
        return std::nullopt;
    }
}

auto ppc_to_tjsample(const int ppc_x, const int ppc_y) -> std::optional<int> {
    switch(ppc_x) {
    case 2:
        switch(ppc_y) {
        case 1:
            return TJSAMP_422;
        case 2:
            return TJSAMP_420;
        }
    }
    WARN("unsupported ppc ", ppc_x, " ", ppc_y);
    return std::nullopt;
}
} // namespace

namespace jpg {
auto BufferDeleter::operator()(std::byte* const buf) -> void {
    tjFree((unsigned char*)buf);
}

auto calc_jpeg_size(const std::byte* const ptr) -> size_t {
    auto p = reinterpret_cast<const uint8_t*>(ptr + 2);
    while(1) {
        const auto marker = *reinterpret_cast<const uint16_t*>(p);
        p += 2;
        const auto seg_size = ((uint8_t)p[0] << 8) + (uint8_t)p[1];
        p += seg_size;
        if(marker == 0xdaff) {
            p += 3;
            break;
        }
    }

loop_sos:
    if(p[0] == 0xff && p[1] == 0xd9) {
        return reinterpret_cast<const std::byte*>(p) - ptr + 2;
    }
    p += 1;
    goto loop_sos;
}

auto decode_jpeg_to_yuvp(const std::byte* const ptr, const size_t len, const size_t downscale_factor) -> std::optional<DecodeResult> {
    auto tj = AutoTJHandle(tjInitDecompress());
    assert_o(tj.get() != NULL);

    auto width     = 0;
    auto height    = 0;
    auto subsample = 0;
    assert_o(tjDecompressHeader2(tj.get(), (unsigned char*)ptr, len, &width, &height, &subsample) == 0);
    unwrap_oo(ppc, tjsample_to_ppc(subsample));
    const auto [ppc_x, ppc_y] = ppc;

    const auto bufsize_y = tjPlaneSizeYUV(0, width / downscale_factor, 0, height / downscale_factor, subsample);
    const auto bufsize_u = tjPlaneSizeYUV(1, width / downscale_factor, 0, height / downscale_factor, subsample);
    const auto bufsize_v = tjPlaneSizeYUV(2, width / downscale_factor, 0, height / downscale_factor, subsample);

    auto buf_y = std::vector<std::byte>(bufsize_y);
    auto buf_u = std::vector<std::byte>(bufsize_u);
    auto buf_v = std::vector<std::byte>(bufsize_v);
    auto buf   = std::array{buf_y.data(), buf_u.data(), buf_v.data()};
    assert_o(tjDecompressToYUVPlanes(tj.get(), (unsigned char*)ptr, len, (unsigned char**)(buf.data()), width / downscale_factor, NULL, height / downscale_factor, 0) == 0);

    return DecodeResult{
        std::move(buf_y),
        std::move(buf_u),
        std::move(buf_v),
        ppc_x,
        ppc_y,
    };
}

auto encode_yuvp_to_jpeg(const int width, const int height, const int stride, const int ppc_x, const int ppc_y, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> std::optional<EncodeResult> {
    auto tj = AutoTJHandle(tjInitCompress());
    assert_o(tj.get() != NULL);

    auto planes  = std::array<const std::byte*, 3>{y, u, v};
    auto strides = std::array<int, 3>{stride, stride / ppc_x, stride / ppc_x};
    auto buf     = (unsigned char*)(nullptr);
    auto size    = size_t(0);
    unwrap_oo(tjsample, ppc_to_tjsample(ppc_x, ppc_y));
    assert_o(tjCompressFromYUVPlanes(tj.get(),
                                     (const unsigned char**)(planes.data()),
                                     width,
                                     strides.data(),
                                     height,
                                     tjsample,
                                     &buf, &size, 100, 0) == 0);
    return EncodeResult{Buffer((std::byte*)buf), size};
}
} // namespace jpg
