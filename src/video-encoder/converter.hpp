#pragma once
#include <optional>

extern "C" {
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "common.hpp"

namespace ff {
av_declare_autoptr(SwrContext, SwrContext, swr_free);

struct Format {
    int            rate;
    AVSampleFormat format;
    uint64_t       channel_layout_mask;
};

class AudioConverter {
  private:
    Format          from;
    AVChannelLayout from_channel;
    Format          to;
    AVChannelLayout to_channel;
    AutoSwrContext  swr_ctx;
    size_t          processed_samples = 0;

  public:
    auto init(Format from, Format to) -> bool;
    auto convert(const std::byte* buffer, size_t buffer_samples) -> std::optional<AutoAVFrame>;
};
} // namespace ff
