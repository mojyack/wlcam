#include "record-context.hpp"
#include "macros/assert.hpp"

auto RecordContext::init(std::string path, ff::VideoParams vopts, const CommonArgs& args) -> bool {
    auto aopts = ff::AudioParams::create<ff::AudioParamsInternal>(ff::AudioParamsInternal{
        .codec = {
            .name    = std::string(args.audio_codec),
            .options = {},
        },
        .sample_fmt     = AV_SAMPLE_FMT_FLTP,
        .sample_rate    = int(args.audio_sample_rate),
        .channel_layout = AV_CHANNEL_LAYOUT_STEREO,
    });

    auto encoder_params = ff::EncoderParams{
        .output       = std::move(path),
        .video        = std::move(vopts),
        .audio        = std::move(aopts),
        .ffmpeg_debug = args.ffmpeg_debug,
    };
    ensure(encoder.init(std::move(encoder_params)));

    auto recorder_params = pa::Params{
        .sample_format    = pa_parse_sample_format("float32"),
        .sample_rate      = uint32_t(args.audio_sample_rate),
        .samples_per_read = uint32_t(encoder.get_audio_samples_per_push()),
    };
    ensure(recorder.init(std::move(recorder_params)));

    ensure(converter.init(
        {int(args.audio_sample_rate), AV_SAMPLE_FMT_FLT, AV_CH_LAYOUT_STEREO},
        {int(args.audio_sample_rate), AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO}));

    running = true;
    return true;
}

auto RecordContext::init(std::string path, const AVPixelFormat pix_fmt, const int width, const int height, const CommonArgs& args) -> bool {
    return init(std::move(path),
                ff::VideoParams::create<ff::VideoParamsInternal>(ff::VideoParamsInternal{
                    .codec = {
                        .name    = std::string(args.video_codec),
                        .options = {},
                    },
                    .pix_fmt = pix_fmt,
                    .width   = width,
                    .height  = height,
                    .threads = 4,
                    .filter  = std::string(args.video_filter),
                }),
                args);
}

auto RecordContext::init(std::string path, int real_width, int real_height, int coded_width, int coded_height, const CommonArgs& args) -> bool {
    return init(std::move(path),
                ff::VideoParams::create<ff::VideoParamsExternal>(ff::VideoParamsExternal{
                    .real_width   = real_width,
                    .real_height  = real_height,
                    .coded_width  = coded_width,
                    .coded_height = coded_height,
                }),
                args);
}

auto RecordContext::ensure_recording() -> void {
    if(!audio_started.load() && encoder.is_header_written()) {
        audio_started.store(true);
        recorder_thread = std::thread(&RecordContext::recorder_main, this);
    }
}
auto RecordContext::recorder_main() -> bool {
    const auto num_samples_per_push = encoder.get_audio_samples_per_push();
loop:
    if(!running) {
        return true;
    }
    const auto samples = recorder.read_buffer();
    ensure(samples);
    const auto frame = converter.convert(samples->data(), num_samples_per_push);
    ensure(frame);
    encoder.add_audio(frame->get());
    goto loop;
}

RecordContext::~RecordContext() {
    running = false;
    if(recorder_thread.joinable()) {
        recorder_thread.join();
    }
}
