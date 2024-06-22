#include <array>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
}

#include "encoder.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"

namespace ff {
declare_autoptr(AVBufferSrcParameters, AVBufferSrcParameters, av_free);
declare_autoptr(AVString, char, av_free);
av_declare_autoptr(AVDict, AVDictionary, av_dict_free);
av_declare_autoptr(AVFilterInOut, AVFilterInOut, avfilter_inout_free);
av_declare_autoptr(AVPacket, AVPacket, av_packet_free);

const auto us_rational = AVRational{1, 1000000};

auto Encoder::create_video_filter(const VideoParams& params) -> std::optional<VideoFilter> {
    // build filter description
    auto filter_desc = std::string();

    filter_desc += "buffer@source=pixel_aspect=1/1";
    filter_desc += build_string(":width=", params.width);
    filter_desc += build_string(":height=", params.height);
    filter_desc += build_string(":pix_fmt=", params.pix_fmt);
    filter_desc += build_string(":time_base=", us_rational.num, "/", us_rational.den);
    if(!params.filter.empty()) {
        filter_desc += ",";
        filter_desc += params.filter;
    }
    if(use_vaapi) {
        filter_desc += ",";
        filter_desc += "hwupload@hwupload";
    }
    filter_desc += ",";
    filter_desc += "buffersink@sink";

    print("using video filter: ", filter_desc);

    // parse to filter
    auto filter = VideoFilter();
    filter.graph.reset(avfilter_graph_alloc());
    assert_o(filter.graph.get() != NULL);

    assert_o(avfilter_graph_parse_ptr(filter.graph.get(), filter_desc.data(), NULL, NULL, NULL) >= 0);

    filter.source_context = avfilter_graph_get_filter(filter.graph.get(), "buffer@source");
    filter.sink_context   = avfilter_graph_get_filter(filter.graph.get(), "buffersink@sink");

    if(use_vaapi) {
        const auto hwupload     = avfilter_graph_get_filter(filter.graph.get(), "hwupload@hwupload");
        hwupload->hw_device_ctx = av_buffer_ref(hw_bufs.device.get());
    }

    assert_o(avfilter_graph_config(filter.graph.get(), NULL) >= 0);

    // copy pipline output's format to encoder input
    const auto filter_output                 = filter.sink_context->inputs[0];
    video_codec_context->width               = filter_output->w;
    video_codec_context->height              = filter_output->h;
    video_codec_context->pix_fmt             = (AVPixelFormat)filter_output->format;
    video_codec_context->time_base           = filter_output->time_base;
    video_codec_context->framerate           = filter_output->frame_rate;
    video_codec_context->sample_aspect_ratio = filter_output->sample_aspect_ratio;

    return filter;
}

auto Encoder::init_video_stream(const VideoParams& params) -> bool {
    auto options = AutoAVDict();
    for(const auto& opt : params.codec.options) {
        auto ptr = options.release();
        av_dict_set(&ptr, opt[0].data(), opt[1].data(), 0);
        options.reset(ptr);
    }

    unwrap_pb(codec, avcodec_find_encoder_by_name(params.codec.name.data()));
    video_stream = avformat_new_stream(format_context.get(), &codec);
    assert_b(video_stream != NULL);
    video_codec_context = avcodec_alloc_context3(&codec);
    assert_b(video_codec_context != NULL);
    video_codec_context->width        = params.width;
    video_codec_context->height       = params.height;
    video_codec_context->time_base    = us_rational;
    video_codec_context->color_range  = AVCOL_RANGE_JPEG;
    video_codec_context->max_b_frames = params.b_frames;
    video_codec_context->thread_count = params.threads;

    if(use_vaapi) {
        auto       device_context = (AVBufferRef*)(nullptr);
        const auto render_node    = params.render_node.empty() ? NULL : params.render_node.data();
        assert_b(av_hwdevice_ctx_create(&device_context, AV_HWDEVICE_TYPE_VAAPI, render_node, NULL, 0) == 0);
        hw_bufs.device.reset(device_context);
    }

    {
        unwrap_ob_mut(filter, create_video_filter(params));
        video_filter = std::move(filter);
    }
    if(this->params.ffmpeg_debug) {
        auto dump = AutoAVString(avfilter_graph_dump(video_filter.graph.get(), 0));
        print(dump.get());
    }

    if(use_vaapi) {
        video_codec_context->hw_frames_ctx = av_buffer_ref(av_buffersink_get_hw_frames_ctx(video_filter.sink_context));
    }

    if(format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        video_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    auto       options_ptr = options.release();
    const auto ret         = avcodec_open2(video_codec_context, &codec, &options_ptr);
    options.reset(options_ptr);
    assert_b(ret >= 0);

    if(use_vaapi) {
        hw_bufs.frame_context = av_hwframe_ctx_alloc(hw_bufs.device.get());
        assert_b(hw_bufs.frame_context != NULL);

        video_codec_context->hw_device_ctx = hw_bufs.frame_context;
        hw_bufs.frame.reset(av_frame_alloc());
        assert_b(hw_bufs.frame.get() != NULL);
    }

    assert_b(avcodec_parameters_from_context(video_stream->codecpar, video_codec_context) >= 0);
    return true;
}

auto Encoder::init_audio_stream(const AudioParams& params) -> bool {
    auto options = AutoAVDict();
    for(const auto& opt : params.codec.options) {
        auto ptr = options.release();
        av_dict_set(&ptr, opt[0].data(), opt[1].data(), 0);
        options.reset(ptr);
    }

    unwrap_pb(codec, avcodec_find_encoder_by_name(params.codec.name.data()));
    audio_stream = avformat_new_stream(format_context.get(), &codec);
    assert_b(audio_stream != NULL);
    audio_codec_context = avcodec_alloc_context3(&codec);
    assert_b(audio_codec_context != NULL);
    audio_codec_context->ch_layout   = params.channel_layout;
    audio_codec_context->sample_fmt  = params.sample_fmt;
    audio_codec_context->sample_rate = params.sample_rate;
    audio_codec_context->time_base   = us_rational;

    if(format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    auto ret = 0;

    auto options_ptr = options.release();
    ret              = avcodec_open2(audio_codec_context, &codec, &options_ptr);
    options.reset(options_ptr);
    assert_b(ret >= 0);

    ret = avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_context);
    assert_b(ret >= 0);

    return true;
}

auto Encoder::init_codecs() -> bool {
    if(params.video) {
        assert_b(init_video_stream(*params.video));
    }
    if(params.audio) {
        assert_b(init_audio_stream(*params.audio));
    }
    if(params.ffmpeg_debug) {
        av_dump_format(format_context.get(), 0, params.output.data(), 1);
    }
    assert_b(avio_open(&format_context->pb, params.output.data(), AVIO_FLAG_WRITE) >= 0);

    auto dummy = (AVDictionary*)(nullptr);
    assert_b(avformat_write_header(format_context.get(), &dummy) >= 0);
    av_dict_free(&dummy);

    return true;
}

auto Encoder::init(EncoderParams params_) -> bool {
    params    = std::move(params_);
    use_vaapi = params.video && params.video->codec.name.find("vaapi") != std::string::npos;

    if(params.ffmpeg_debug) {
        av_log_set_level(AV_LOG_DEBUG);
    }
    avdevice_register_all();

    output_format = av_guess_format(NULL, params.output.data(), NULL);
    assert_b(output_format != NULL);

    auto fmt_ctx = (AVFormatContext*)(nullptr);
    assert_b(avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, params.output.data()) >= 0);
    format_context.reset(fmt_ctx);

    assert_b(init_codecs());

    init_done = true;
    return true;
}

auto Encoder::encode(AVFrame* const frame, AVPacket* packet, bool video) -> bool {
    const auto ctx    = video ? video_codec_context : audio_codec_context;
    const auto stream = video ? video_stream : audio_stream;
    const auto guard  = std::lock_guard(video ? video_encode_lock : audio_encode_lock);

    assert_b(avcodec_send_frame(ctx, frame) >= 0);
    if(const auto ret = avcodec_receive_packet(ctx, packet); ret < 0) {
        return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF;
    }

    av_packet_rescale_ts(packet, ctx->time_base, stream->time_base);
    packet->stream_index = stream->index;
    assert_b(av_interleaved_write_frame(format_context.get(), packet) >= 0);
    return true;
}

auto Encoder::push_frame(AutoAVFrame frame, const int usec) -> bool {
    frame->pts = usec;

    auto filtered = AutoAVFrame(av_frame_alloc());
    assert_b(filtered.get() != NULL);
    {
        auto filter_guard = std::lock_guard(filter_lock);
        assert_b(av_buffersrc_add_frame_flags(video_filter.source_context, frame.get(), 0) >= 0);
        assert_b(av_buffersink_get_frame(video_filter.sink_context, filtered.get()) >= 0);
    }
    filtered->pict_type = AV_PICTURE_TYPE_NONE;

    auto pkt  = AutoAVPacket(av_packet_alloc());
    pkt->data = NULL;
    pkt->size = 0;
    assert_b(encode(filtered.get(), pkt.get(), true));
    return true;
}

auto Encoder::add_frame(std::span<const Plane> planes, const int usec) -> bool {
    unwrap_ob(params, this->params.video);
    assert_b(planes.size() <= AV_NUM_DATA_POINTERS);

    auto frame = AutoAVFrame(av_frame_alloc());
    assert_b(frame.get() != NULL);

    for(auto i = 0u; i < planes.size(); i += 1) {
        frame->data[i]     = std::bit_cast<uint8_t*>(planes[i].data);
        frame->linesize[i] = planes[i].stride;
    }
    frame->format = params.pix_fmt;
    frame->width  = params.width;
    frame->height = params.height;

    return push_frame(std::move(frame), usec);
}

auto Encoder::get_audio_samples_per_push() const -> size_t {
    return audio_codec_context->frame_size;
}

auto Encoder::add_audio(AVFrame* frame) -> bool {
    auto pkt  = AutoAVPacket(av_packet_alloc());
    pkt->data = NULL;
    pkt->size = 0;
    return encode(frame, pkt.get(), false);
}

auto Encoder::add_audio(const std::span<const std::byte* const> buffers) -> bool {
    unwrap_ob(params, this->params.audio);
    assert_b(buffers.size() == size_t(audio_codec_context->ch_layout.nb_channels));

    auto frame = AutoAVFrame(av_frame_alloc());
    assert_b(frame.get() != NULL);

    for(auto i = 0; i < audio_codec_context->ch_layout.nb_channels; i += 1) {
        frame->data[i] = std::bit_cast<uint8_t*>(buffers[i]);
    }
    frame->sample_rate = params.sample_rate;
    frame->format      = params.sample_fmt;
    frame->ch_layout   = audio_codec_context->ch_layout;
    frame->nb_samples  = audio_codec_context->frame_size;

    return add_audio(frame.get());
}

Encoder::~Encoder() {
    if(!init_done) {
        return;
    }

    auto pkt = AutoAVPacket(av_packet_alloc());
    if(params.video) {
        encode(NULL, pkt.get(), true);
    }
    if(params.audio) {
        encode(NULL, pkt.get(), false);
    }

    av_write_trailer(format_context.get());
    if(output_format && (!(output_format->flags & AVFMT_NOFILE))) {
        avio_closep(&format_context->pb);
    }
}
} // namespace ff
