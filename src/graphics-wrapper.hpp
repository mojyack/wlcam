#pragma once
#include <span>
#include <string_view>

#include "encoder/encoder.hpp"
#include "gawl/screen.hpp"
#include "graphics/planar.hpp"
#include "graphics/yuv420sp.hpp"
#include "graphics/yuv422i.hpp"
#include "jpeg.hpp"

class Frame {
  public:
    using ByteArray = std::span<const std::byte>;

    virtual auto save_to_jpeg(ByteArray buf, std::string_view path) -> bool               = 0;
    virtual auto load_texture(ByteArray buf) -> bool                                      = 0;
    virtual auto draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void = 0;
    virtual auto get_pixel_format() const -> std::optional<AVPixelFormat>                 = 0;
    virtual auto get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> = 0;

    virtual ~Frame() {}
};

class JpegFrame : public Frame {
  private:
    PlanarGraphic                    graphic;
    std::optional<jpg::DecodeResult> decoded;

  public:
    auto save_to_jpeg(ByteArray buf, std::string_view path) -> bool override;
    auto load_texture(ByteArray buf) -> bool override;
    auto draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void override;
    auto get_pixel_format() const -> std::optional<AVPixelFormat> override;
    auto get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> override;
};

class YUV422IFrame : public Frame {
  private:
    int            width;
    int            height;
    int            stride;
    YUV422iGraphic graphic;

  public:
    auto save_to_jpeg(ByteArray buf, std::string_view path) -> bool override;
    auto load_texture(ByteArray buf) -> bool override;
    auto draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void override;
    auto get_pixel_format() const -> std::optional<AVPixelFormat> override;
    auto get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> override;

    YUV422IFrame(int width, int height, int stride);
};

class YUV420SPFrame : public Frame {
  private:
    int             width;
    int             height;
    int             stride;
    YUV420spGraphic graphic;

  public:
    auto save_to_jpeg(ByteArray buf, std::string_view path) -> bool override;
    auto load_texture(ByteArray buf) -> bool override;
    auto draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void override;
    auto get_pixel_format() const -> std::optional<AVPixelFormat> override;
    auto get_planes(ByteArray buf) const -> std::optional<std::vector<ff::Plane>> override;

    YUV420SPFrame(int width, int height, int stride);
};
