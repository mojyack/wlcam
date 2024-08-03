#include "record-context.hpp"
#include "macros/assert.hpp"
#include "util/assert.hpp"

auto RecordContext::init(std::string path, const AVPixelFormat pix_fmt, const CommonArgs& args) -> bool {
    auto encoder_params = ff::EncoderParams{
        .output = std::move(path),
        .video  = ff::VideoParams{
             .codec = {
                 .name    = std::string(args.video_codec),
                 .options = {}, // TODO: make configurable
            },
             .pix_fmt = pix_fmt,
             .width   = int(args.width),
             .height  = int(args.height),
             .threads = 4, // TODO: make configurable
             .filter  = std::string(args.video_filter),
        },
        .audio = ff::AudioParams{
            .codec = {
                .name    = std::string(args.audio_codec),
                .options = {},
            },
            .sample_fmt     = AV_SAMPLE_FMT_FLTP,
            .sample_rate    = int(args.audio_sample_rate),
            .channel_layout = AV_CHANNEL_LAYOUT_STEREO,
        },
        .ffmpeg_debug = args.ffmpeg_debug,
    };
    assert_b(encoder.init(std::move(encoder_params)));

    auto recorder_params = pa::Params{
        .sample_format    = pa_parse_sample_format("float32"),
        .sample_rate      = uint32_t(args.audio_sample_rate),
        .samples_per_read = uint32_t(encoder.get_audio_samples_per_push()),
    };
    assert_b(recorder.init(std::move(recorder_params)));

    assert_b(converter.init(
        {int(args.audio_sample_rate), AV_SAMPLE_FMT_FLT, AV_CH_LAYOUT_STEREO},
        {int(args.audio_sample_rate), AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO}));

    running         = true;
    recorder_thread = std::thread(&RecordContext::recorder_main, this);

    return true;
}

auto RecordContext::recorder_main() -> bool {
    const auto num_samples_per_push = encoder.get_audio_samples_per_push();
loop:
    if(!running) {
        return true;
    }
    const auto samples = recorder.read_buffer();
    assert_b(samples);
    const auto frame = converter.convert(samples->data(), num_samples_per_push);
    assert_b(frame);
    encoder.add_audio(frame->get());
    goto loop;
}

RecordContext::~RecordContext() {
    running = false;
    recorder_thread.join();
}
