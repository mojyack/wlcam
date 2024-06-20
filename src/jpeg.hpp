#pragma once
#include <optional>
#include <vector>

namespace jpg {
struct BufferDeleter {
    auto operator()(std::byte* buf) -> void;
};

using Buffer = std::unique_ptr<std::byte, BufferDeleter>;

struct DecodeResult {
    std::vector<std::byte> y;
    std::vector<std::byte> u;
    std::vector<std::byte> v;
    int                    ppc_x;
    int                    ppc_y;
};

struct EncodeResult {
    Buffer buffer;
    size_t size;
};

auto calc_jpeg_size(const std::byte* ptr) -> size_t;
auto decode_jpeg_to_yuvp(const std::byte* const ptr, size_t len, size_t downscale_factor) -> std::optional<DecodeResult>;
auto encode_yuvp_to_jpeg(int width, int height, int stride, int ppc_x, int ppc_y, const std::byte* y, const std::byte* u, const std::byte* v) -> std::optional<EncodeResult>;
} // namespace jpg
