#include "converter.hpp"

#include "macros/assert.hpp"
#include "util/assert.hpp"

namespace ff {
auto AudioConverter::init(Format from_, Format to_) -> bool {
    from = from_;
    to   = to_;

    assert_b(av_channel_layout_from_mask(&from_channel, from.channel_layout_mask) == 0);
    assert_b(av_channel_layout_from_mask(&to_channel, to.channel_layout_mask) == 0);

    swr_ctx.reset(swr_alloc());
    assert_b(swr_ctx.get() != NULL);

    assert_b(av_opt_set_int(swr_ctx.get(), "in_sample_rate", from.rate, 0) == 0);
    assert_b(av_opt_set_int(swr_ctx.get(), "out_sample_rate", to.rate, 0) == 0);
    assert_b(av_opt_set_sample_fmt(swr_ctx.get(), "in_sample_fmt", from.format, 0) == 0);
    assert_b(av_opt_set_sample_fmt(swr_ctx.get(), "out_sample_fmt", to.format, 0) == 0);
    assert_b(av_opt_set_chlayout(swr_ctx.get(), "in_chlayout", &from_channel, 0) == 0);
    assert_b(av_opt_set_chlayout(swr_ctx.get(), "out_chlayout", &to_channel, 0) == 0);
    assert_b(swr_init(swr_ctx.get()) == 0);

    return true;
}

auto AudioConverter::convert(const std::byte* const buffer, const size_t buffer_samples) -> std::optional<AutoAVFrame> {
    auto inputf         = AutoAVFrame(av_frame_alloc());
    inputf->data[0]     = (uint8_t*)buffer;
    inputf->sample_rate = from.rate;
    inputf->format      = from.format;
    inputf->ch_layout   = from_channel;
    inputf->nb_samples  = buffer_samples;

    auto outputf         = AutoAVFrame(av_frame_alloc());
    outputf->sample_rate = to.rate;
    outputf->format      = to.format;
    outputf->ch_layout   = to_channel;
    outputf->nb_samples  = buffer_samples;
    outputf->pts         = size_t(1000000) * processed_samples / to.rate;

    processed_samples += buffer_samples;

    assert_o(swr_convert_frame(swr_ctx.get(), outputf.get(), inputf.get()) == 0);
    return outputf;
}
} // namespace ff
