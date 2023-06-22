#include <array>
#include <vector>

#include <stdio.h>
#include <turbojpeg.h>

#include "../assert.hpp"
#include "../pixel-buffer.hpp"

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

class YUVImage : public Image {
  private:
    int                    width;
    int                    height;
    int                    ppc_x;
    int                    ppc_y;
    std::vector<std::byte> y;
    std::vector<std::byte> u;
    std::vector<std::byte> v;

  public:
    auto get_plane(const int num) -> PixelBuffer override {
        switch(num) {
        case 0:
            return {width, height, y.data()};
        case 1:
            return {width / ppc_x, height / ppc_y, u.data()};
        case 2:
            return {width / ppc_x, height / ppc_y, v.data()};
        }
        PANIC("no such plane");
    }

    YUVImage(const int width, const int height, const int ppc_x, const int ppc_y, std::vector<std::byte> y, std::vector<std::byte> u, std::vector<std::byte> v)
        : width(width),
          height(height),
          ppc_x(ppc_x),
          ppc_y(ppc_y),
          y(std::move(y)),
          u(std::move(u)),
          v(std::move(v)) {}
};

auto decode_jpeg_yuv422_planar(const std::byte* const ptr, const size_t len) -> std::unique_ptr<Image> {
    constexpr auto size_denominator = 4;

    const auto tj = tjInitDecompress();
    DYN_ASSERT(tj != NULL);

    int width;
    int height;
    int subsample;
    int ppc_x; // pixel per chrominance
    int ppc_y;
    DYN_ASSERT(tjDecompressHeader2(tj, (unsigned char*)ptr, len, &width, &height, &subsample) == 0);
    switch(subsample) {
    case TJSAMP_422:
        ppc_x = 2;
        ppc_y = 1;
        break;
    case TJSAMP_420:
        ppc_x = 2;
        ppc_y = 2;
        break;
    default:
        PANIC("unsupported sampling type ", subsample);
    }

    const auto bufsize_y = tjPlaneSizeYUV(0, width / size_denominator, 0, height / size_denominator, subsample);
    const auto bufsize_u = tjPlaneSizeYUV(1, width / size_denominator, 0, height / size_denominator, subsample);
    const auto bufsize_v = tjPlaneSizeYUV(2, width / size_denominator, 0, height / size_denominator, subsample);

    auto buf_y = std::vector<std::byte>(bufsize_y);
    auto buf_u = std::vector<std::byte>(bufsize_u);
    auto buf_v = std::vector<std::byte>(bufsize_v);
    auto buf   = std::array{buf_y.data(), buf_u.data(), buf_v.data()};
    DYN_ASSERT(tjDecompressToYUVPlanes(tj, (unsigned char*)ptr, len, (unsigned char**)(buf.data()), width / size_denominator, NULL, height / size_denominator, 0) == 0);
    tjDestroy(tj);

    return std::make_unique<YUVImage>(width / size_denominator, height / size_denominator, ppc_y, ppc_x, std::move(buf_y), std::move(buf_u), std::move(buf_v));
}
