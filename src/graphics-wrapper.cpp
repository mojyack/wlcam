#include <fstream>

#include "graphics-wrapper.hpp"
#include "jpeg.hpp"
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
    unwrap_ob(bufs, jpg::decode_jpeg_to_yuvp(buf.data(), buf.size(), 1));
    graphic.update_texture(bufs.width, bufs.height, 0, bufs.ppc_x, bufs.ppc_y, bufs.y.data(), bufs.u.data(), bufs.v.data());
    return true;
}

auto JpegFrame::draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    graphic.draw_fit_rect(screen, rect);
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

YUV420SPFrame::YUV420SPFrame(const int width, const int height, const int stride)
    : width(width),
      height(height),
      stride(stride) {
}
