#include "macros/assert.hpp"
#include "util/assert.hpp"

#include "pulse.hpp"

namespace pa {
constexpr auto channels = 2;

auto Recorder::init(Params params_) -> bool {
    params = std::move(params_);

    sample_format_size = pa_sample_size_of_format(params.sample_format);

    auto map = pa_channel_map();
    pa_channel_map_init_stereo(&map);

    auto attr      = pa_buffer_attr();
    attr.maxlength = params.samples_per_read * channels * sample_format_size;
    attr.fragsize  = attr.maxlength;

    auto spec = pa_sample_spec{
        .format   = params.sample_format,
        .rate     = params.sample_rate,
        .channels = channels,
    };

    const auto dev = params.device.empty() ? "default" : params.device.data();
    pa.reset(pa_simple_new(NULL, params.client_name.data(), PA_STREAM_RECORD, dev, "capture", &spec, &map, &attr, NULL));
    assert_b(pa.get() != NULL);

    return true;
}

auto Recorder::read_buffer() -> std::optional<std::vector<std::byte>> {
    auto buf = std::vector<std::byte>(params.samples_per_read * channels * sample_format_size);
    assert_o(pa_simple_read(pa.get(), buf.data(), buf.size(), NULL) == 0);
    return buf;
}
} // namespace pa
