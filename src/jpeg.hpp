#pragma once
#include <vector>

namespace jpg {
struct TJDeleter {
    auto operator()(unsigned char* const buf) -> void;
};

using TJBuffer = std::unique_ptr<unsigned char, TJDeleter>;

struct DecodeResult {
    std::vector<std::byte> y;
    std::vector<std::byte> u;
    std::vector<std::byte> v;
    int                    ppc_x;
    int                    ppc_y;
};

auto calc_jpeg_size(const std::byte* ptr) -> size_t;

auto decode_jpeg_to_yuvp(const std::byte* const ptr, const size_t len, const size_t downscale_factor) -> DecodeResult;

auto encode_yuvp_to_jpeg(int width, int height, int stride, int ppc_x, int ppc_y, const std::byte* y, const std::byte* u, const std::byte* v) -> std::pair<TJBuffer, size_t>;
} // namespace jpg
