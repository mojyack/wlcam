#include <fstream>

#include "graphics-wrapper.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"
#include "yuv.hpp"

namespace {
auto save_yuvp_frame(const char* const path, const int width, const int height, const int stride, const int ppc_x, const int ppc_y, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> bool {
    unwrap_ob(jpeg, jpg::encode_yuvp_to_jpeg(width, height, stride, ppc_x, ppc_y, y, u, v));
    auto file = std::ofstream(path, std::ios::out | std::ios::binary);
    file.write((const char*)jpeg.buffer.get(), jpeg.size);
    return true;
}
} // namespace

// JpegFrame
auto JpegFrame::save_to_jpeg(const ByteArray buf, std::string_view const path) -> bool {
    auto       file = std::ofstream(path, std::ios::out | std::ios::binary);
    const auto size = jpg::calc_jpeg_size(buf.data());
    file.write((const char*)buf.data(), size);
    return true;
}

auto JpegFrame::load_texture(const ByteArray buf) -> bool {
    decoded = jpg::decode_jpeg_to_yuvp(buf.data(), buf.size(), 1);
    assert_b(decoded);
    graphic.update_texture(decoded->width, decoded->height, 0, decoded->ppc_x, decoded->ppc_y, decoded->y.data(), decoded->u.data(), decoded->v.data());
    return true;
}

auto JpegFrame::draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    graphic.draw_fit_rect(screen, rect);
}

auto JpegFrame::get_pixel_format() const -> std::optional<AVPixelFormat> {
    assert_o(decoded);
    switch(decoded->ppc_x) {
    case 2:
        switch(decoded->ppc_y) {
        case 1:
            return AV_PIX_FMT_YUV422P;
        case 2:
            return AV_PIX_FMT_YUV420P;
        }
    }
    return std::nullopt;
}

auto JpegFrame::get_planes(ByteArray /*buf*/) const -> std::optional<std::vector<ff::Plane>> {
    assert_o(decoded);
    const auto p1 = ff::Plane{decoded->y.data(), decoded->width};
    const auto p2 = ff::Plane{decoded->u.data(), decoded->width / 2};
    const auto p3 = ff::Plane{decoded->v.data(), decoded->width / 2};
    return std::vector{p1, p2, p3};
}

// YUV422IFrame
auto YUV422IFrame::save_to_jpeg(const ByteArray buf, std::string_view const path) -> bool {
    const auto [ybuf, ubuf, vbuf] = yuv::yuv422i_to_yuv422p(buf.data(), width, height, stride);
    assert_b(save_yuvp_frame(path.data(), width, height, stride / 2, 2, 1, ybuf.data(), ubuf.data(), vbuf.data()));
    return true;
}

auto YUV422IFrame::load_texture(ByteArray buf) -> bool {
    graphic.update_texture(width, height, stride, buf.data());
    return true;
}

auto YUV422IFrame::draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    graphic.draw_fit_rect(screen, rect);
}

auto YUV422IFrame::get_pixel_format() const -> std::optional<AVPixelFormat> {
    return AV_PIX_FMT_YUYV422;
}

auto YUV422IFrame::get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> {
    const auto p1 = ff::Plane{buf.data(), stride};
    return std::vector{p1};
}

YUV422IFrame::YUV422IFrame(const int width, const int height, const int stride)
    : width(width),
      height(height),
      stride(stride) {
}

// YUV420SPFrame
auto YUV420SPFrame::save_to_jpeg(const ByteArray buf, std::string_view const path) -> bool {
    const auto uvbuf = buf.data() + stride * height;
    auto       ubuf  = std::vector<std::byte>(height / 4 * width);
    auto       vbuf  = std::vector<std::byte>(height / 4 * width);

    yuv::yuv420sp_uvsp_to_uvp(uvbuf, ubuf.data(), vbuf.data(), width, height, stride);
    assert_b(save_yuvp_frame(path.data(), width, height, stride, 2, 2, buf.data(), ubuf.data(), vbuf.data()));
    return true;
}

auto YUV420SPFrame::load_texture(ByteArray buf) -> bool {
    graphic.update_texture(width, height, stride, buf.data(), buf.data() + stride * height);
    return true;
}

auto YUV420SPFrame::draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    graphic.draw_fit_rect(screen, rect);
}

auto YUV420SPFrame::get_pixel_format() const -> std::optional<AVPixelFormat> {
    return AV_PIX_FMT_NV12;
}

auto YUV420SPFrame::get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> {
    const auto uvbuf = buf.data() + stride * height;
    const auto p1    = ff::Plane{buf.data(), stride};
    const auto p2    = ff::Plane{uvbuf, stride};
    return std::vector{p1, p2};
}

YUV420SPFrame::YUV420SPFrame(const int width, const int height, const int stride)
    : width(width),
      height(height),
      stride(stride) {
}
