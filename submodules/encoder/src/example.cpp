#include "encoder/converter.hpp"
#include "encoder/encoder.hpp"
#include "macros/assert.hpp"
#include "pulse/pulse.hpp"
#include "util/assert.hpp"
#include "util/misc.hpp"

auto main(const int argc, const char* const argv[]) -> int {
    constexpr auto video       = true;
    constexpr auto audio       = true;
    constexpr auto width       = 300;
    constexpr auto height      = 200;
    constexpr auto duration    = 5;
    constexpr auto sample_rate = 48000;

    auto params = ff::EncoderParams{
        .output = "/tmp/output.mkv",
        .video  = ff::VideoParams{
             .codec = {
                 .name    = "libx264",
                 .options = {},
            },
             .pix_fmt = AV_PIX_FMT_NV12,
             .width   = width,
             .height  = height,
        },
        .audio = ff::AudioParams{
            .codec = {
                .name    = "aac",
                .options = {},
            },
            .sample_fmt     = AV_SAMPLE_FMT_FLTP,
            .sample_rate    = sample_rate,
            .channel_layout = AV_CHANNEL_LAYOUT_STEREO,
        },
        .ffmpeg_debug = true,
    };

    if(!video) {
        params.video = std::nullopt;
    }
    if(!audio) {
        params.audio = std::nullopt;
    }

    if(argc >= 2) {
        print("using vappi");
        params.video->codec.name = "h264_vaapi";
    }

    auto encoder = ff::Encoder(std::move(params));
    assert_v(encoder.init(), 1);

    auto converter = ff::AudioConverter({sample_rate, AV_SAMPLE_FMT_FLT, AV_CH_LAYOUT_STEREO}, {sample_rate, AV_SAMPLE_FMT_FLTP, AV_CH_LAYOUT_STEREO});
    assert_v(converter.init(), 1);

    auto buf1 = std::vector<std::byte>(width * height);
    auto buf2 = std::vector<std::byte>(width * height);
    auto bufz = std::vector<std::byte>(width * height);
    auto ps1  = std::array<ff::Plane, 2>();
    auto ps2  = std::array<ff::Plane, 2>();
    ps1[0]    = ff::Plane{buf1.data(), width};
    ps1[1]    = ff::Plane{bufz.data(), width};
    ps2[0]    = ff::Plane{buf2.data(), width};
    ps2[1]    = ff::Plane{bufz.data(), width};
    memset(bufz.data(), 0, bufz.size());
    for(auto y = 0; y < height; y += 1) {
        for(auto x = 0; x < width; x += 1) {
            buf1[y * width + x] = std::byte(255.0 * x / width);
            buf2[y * width + x] = std::byte(255.0 * y / height);
        }
    }

    for(auto i = 0; i < duration * 100; i += 1) {
        encoder.add_frame(i % 2 == 0 ? ps1 : ps2, i * 1000 * 10);
    }

    const auto num_samples_per_push = encoder.get_audio_samples_per_push();

    auto recorder = pa::Recorder(pa::Params{
        .sample_format    = pa_parse_sample_format("float32"),
        .sample_rate      = sample_rate,
        .samples_per_read = uint32_t(num_samples_per_push),
    });
    assert_v(recorder.init(), 1);

    for(auto i = 0u; i < duration * sample_rate / num_samples_per_push; i += 1) {
        const auto samples = recorder.read_buffer();
        assert_v(samples, 1);
        const auto frame = converter.convert(samples->data(), num_samples_per_push);
        assert_v(frame, 1);
        encoder.add_audio(frame->get());
    }

    return 0;
}
