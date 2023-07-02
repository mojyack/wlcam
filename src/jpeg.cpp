#include <array>
#include <vector>

#include <gawl/pixelbuffer.hpp>
#include <stdio.h>
#include <turbojpeg.h>

#include "../assert.hpp"
#include "jpeg.hpp"

namespace {
// pixel per chrominance
auto tjsample_to_ppc(const int sample) -> std::array<int, 2> {
    switch(sample) {
    case TJSAMP_422:
        return {2, 1};
        break;
    case TJSAMP_420:
        return {2, 2};
        break;
    default:
        PANIC("unsupported sampling type ", sample);
    }
}

auto ppc_to_tjsample(const int ppc_x, const int ppc_y) -> int {
    switch(ppc_x) {
    case 2:
        switch(ppc_y) {
        case 1:
            return TJSAMP_422;
        case 2:
            return TJSAMP_420;
        }
    }
    PANIC("unsupported ppc");
}
} // namespace

namespace jpg {
auto TJDeleter::operator()(unsigned char* buf) -> void {
    tjFree(buf);
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

auto decode_jpeg_to_yuvp(const std::byte* const ptr, const size_t len) -> std::array<gawl::PixelBuffer, 3> {
    constexpr auto size_denominator = 4;

    const auto tj = tjInitDecompress();
    DYN_ASSERT(tj != NULL);

    int width;
    int height;
    int subsample;
    DYN_ASSERT(tjDecompressHeader2(tj, (unsigned char*)ptr, len, &width, &height, &subsample) == 0);
    const auto [ppc_x, ppc_y] = tjsample_to_ppc(subsample);

    const auto bufsize_y = tjPlaneSizeYUV(0, width / size_denominator, 0, height / size_denominator, subsample);
    const auto bufsize_u = tjPlaneSizeYUV(1, width / size_denominator, 0, height / size_denominator, subsample);
    const auto bufsize_v = tjPlaneSizeYUV(2, width / size_denominator, 0, height / size_denominator, subsample);

    auto buf_y = std::vector<std::byte>(bufsize_y);
    auto buf_u = std::vector<std::byte>(bufsize_u);
    auto buf_v = std::vector<std::byte>(bufsize_v);
    auto buf   = std::array{buf_y.data(), buf_u.data(), buf_v.data()};
    DYN_ASSERT(tjDecompressToYUVPlanes(tj, (unsigned char*)ptr, len, (unsigned char**)(buf.data()), width / size_denominator, NULL, height / size_denominator, 0) == 0);
    tjDestroy(tj);

    return {
        gawl::PixelBuffer::from_raw(width / size_denominator, height / size_denominator, std::move(buf_y)),
        gawl::PixelBuffer::from_raw(width / size_denominator / ppc_x, height / size_denominator / ppc_y, std::move(buf_u)),
        gawl::PixelBuffer::from_raw(width / size_denominator / ppc_x, height / size_denominator / ppc_y, std::move(buf_v)),
    };
}

auto encode_yuvp_to_jpeg(const int width, const int height, const int stride, const int ppc_x, const int ppc_y, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> std::pair<TJBuffer, size_t> {
    const auto tj = tjInitCompress();
    DYN_ASSERT(tj != NULL);
    auto planes  = std::array<const std::byte*, 3>{y, u, v};
    auto strides = std::array<int, 3>{stride, stride / ppc_x, stride / ppc_x};
    auto buf     = (unsigned char*)(nullptr);
    auto size    = size_t(0);
    DYN_ASSERT(tjCompressFromYUVPlanes(tj,
                                       (const unsigned char**)(planes.data()),
                                       width,
                                       strides.data(),
                                       height,
                                       ppc_to_tjsample(ppc_x, ppc_y),
                                       &buf, &size, 100, 0) == 0);
    return {TJBuffer(buf), size};
}
} // namespace jpg
