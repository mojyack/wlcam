extern "C" {
#include <libavdevice/avdevice.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
#include <libavutil/pixdesc.h>
}

#include "../macros/unwrap.hpp"
#include "encoder.hpp"

namespace ff {
namespace {
declare_autoptr(AVBufferSrcParameters, AVBufferSrcParameters, av_free);
declare_autoptr(AVString, char, av_free);
av_declare_autoptr(AVDict, AVDictionary, av_dict_free);
av_declare_autoptr(AVFilterInOut, AVFilterInOut, avfilter_inout_free);
av_declare_autoptr(AVPacket, AVPacket, av_packet_free);

constexpr auto us_rational = AVRational{1, 1000000};

auto derive_h264_extradata(const std::span<const uint8_t> annexb) -> std::vector<uint8_t> {
    auto       ret = std::vector<uint8_t>();
    const auto u   = annexb.data();
    const auto n   = annexb.size();
    for(auto i = 0uz; i + 4 < n;) {
        // find a start code (3- or 4-byte)
        auto sc = size_t(0);
        if(u[i] == 0 && u[i + 1] == 0 && u[i + 2] == 1) {
            sc = 3;
        } else if(u[i] == 0 && u[i + 1] == 0 && u[i + 2] == 0 && u[i + 3] == 1) {
            sc = 4;
        } else {
            i += 1;
            continue;
        }
        const auto nal_start = i + sc;
        auto       j         = nal_start;
        for(; j + 3 < n; j += 1) {
            if(u[j] == 0 && u[j + 1] == 0 && (u[j + 2] == 1 || (u[j + 2] == 0 && j + 3 < n && u[j + 3] == 1))) {
                break;
            }
        }
        const auto nal_end = (j + 3 < n) ? j : n;
        const auto type    = u[nal_start] & 0x1f;
        if(type == 7 || type == 8) { // SPS / PPS -> keep (with 4-byte start code)
            const uint8_t sc4[] = {0, 0, 0, 1};
            ret.insert(ret.end(), sc4, sc4 + 4);
            ret.insert(ret.end(), u + nal_start, u + nal_end);
        }
        i = nal_end;
    }
    return ret;
}
} // namespace

auto Encoder::create_video_filter(const VideoParamsInternal& params, InternalVideoContext::HWBuffers& hw_bufs, AVCodecContext& codec_context) -> std::optional<VideoFilter> {
    // build filter description
    auto filter_desc = std::string();

    filter_desc += "buffer@source=pixel_aspect=1/1";
    filter_desc += std::format(":width={}", params.width);
    filter_desc += std::format(":height={}", params.height);
    filter_desc += std::format(":pix_fmt={}", int(params.pix_fmt));
    filter_desc += std::format(":time_base={}/{}", us_rational.num, us_rational.den);
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

    std::println("using video filter: {}", filter_desc);

    // parse to filter
    auto filter = VideoFilter();
    filter.graph.reset(avfilter_graph_alloc());
    ensure(filter.graph.get() != NULL);

    ensure(avfilter_graph_parse_ptr(filter.graph.get(), filter_desc.data(), NULL, NULL, NULL) >= 0);

    filter.source_context = avfilter_graph_get_filter(filter.graph.get(), "buffer@source");
    filter.sink_context   = avfilter_graph_get_filter(filter.graph.get(), "buffersink@sink");

    if(use_vaapi) {
        const auto hwupload     = avfilter_graph_get_filter(filter.graph.get(), "hwupload@hwupload");
        hwupload->hw_device_ctx = av_buffer_ref(hw_bufs.device.get());
    }

    ensure(avfilter_graph_config(filter.graph.get(), NULL) >= 0);

    // copy pipline output's format to encoder input
    const auto filter_output          = filter.sink_context->inputs[0];
    codec_context.width               = filter_output->w;
    codec_context.height              = filter_output->h;
    codec_context.pix_fmt             = (AVPixelFormat)filter_output->format;
    codec_context.time_base           = filter_output->time_base;
    codec_context.sample_aspect_ratio = filter_output->sample_aspect_ratio;

    return filter;
}

auto Encoder::setup_crop_bsf(const VideoParamsExternal& params, const char* const bsf_name, AVStream& stream) -> std::optional<AutoAVBSFContext> {
    auto ret = AutoAVBSFContext();

    const auto pad_w = params.coded_width - params.real_width;
    const auto pad_h = params.coded_height - params.real_height;
    if(pad_w <= 0 && pad_h <= 0) {
        return ret;
    }

    unwrap(bsf, av_bsf_get_by_name(bsf_name));
    ensure(av_bsf_alloc(&bsf, std::inout_ptr(ret)) >= 0);
    ensure(avcodec_parameters_copy(ret->par_in, stream.codecpar) >= 0);
    ret->time_base_in = us_rational;
    if(pad_w > 0) {
        av_opt_set_int(ret.get(), "crop_right", pad_w, AV_OPT_SEARCH_CHILDREN);
    }
    if(pad_h > 0) {
        av_opt_set_int(ret.get(), "crop_bottom", pad_h, AV_OPT_SEARCH_CHILDREN);
    }
    ensure(av_bsf_init(ret.get()) >= 0);

    return ret;
}

auto Encoder::init_video_stream_internal(const VideoParamsInternal& params) -> std::optional<InternalVideoContext> {
    use_vaapi = params.codec.name.find("vaapi") != std::string::npos;

    auto options = AutoAVDict();
    for(const auto& opt : params.codec.options) {
        av_dict_set(std::inout_ptr(options), opt[0].data(), opt[1].data(), 0);
    }

    unwrap(codec, avcodec_find_encoder_by_name(params.codec.name.data()));
    unwrap_mut(stream, avformat_new_stream(format_context.get(), NULL));
    unwrap_mut(codec_context, avcodec_alloc_context3(&codec));
    codec_context.width        = params.width;
    codec_context.height       = params.height;
    codec_context.time_base    = us_rational;
    codec_context.color_range  = AVCOL_RANGE_JPEG;
    codec_context.max_b_frames = params.b_frames;
    codec_context.thread_count = params.threads;

    auto hw_bufs = InternalVideoContext::HWBuffers();
    if(use_vaapi) {
        auto       device_context = (AVBufferRef*)(nullptr);
        const auto render_node    = params.render_node.empty() ? NULL : params.render_node.data();
        ensure(av_hwdevice_ctx_create(&device_context, AV_HWDEVICE_TYPE_VAAPI, render_node, NULL, 0) == 0);
        hw_bufs.device.reset(device_context);
    }

    unwrap_mut(filter, create_video_filter(params, hw_bufs, codec_context));

    if(this->params.ffmpeg_debug) {
        auto dump = AutoAVString(avfilter_graph_dump(filter.graph.get(), 0));
        std::println("{}", dump.get());
    }
    if(use_vaapi) {
        codec_context.hw_frames_ctx = av_buffer_ref(av_buffersink_get_hw_frames_ctx(filter.sink_context));
    }

    if(format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_context.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ensure(avcodec_open2(&codec_context, &codec, std::inout_ptr(options)) >= 0);

    if(use_vaapi) {
        hw_bufs.frame_context = av_hwframe_ctx_alloc(hw_bufs.device.get());
        ensure(hw_bufs.frame_context != NULL);

        codec_context.hw_device_ctx = hw_bufs.frame_context;
        hw_bufs.frame.reset(av_frame_alloc());
        ensure(hw_bufs.frame.get() != NULL);
    }

    ensure(avcodec_parameters_from_context(stream.codecpar, &codec_context) >= 0);
    return InternalVideoContext{
        .stream        = &stream,
        .codec_context = &codec_context,
        .filter        = std::move(filter),
        .hw_bufs       = std::move(hw_bufs),
    };
}

auto Encoder::init_video_stream_external(const VideoParamsExternal& params) -> std::optional<ExternalVideoContext> {
    unwrap_mut(stream, avformat_new_stream(format_context.get(), NULL));

    auto& par      = *stream.codecpar;
    par.codec_type = AVMEDIA_TYPE_VIDEO;
    par.codec_id   = AV_CODEC_ID_H264;
    par.width      = params.real_width;
    par.height     = params.real_height;
    par.format     = AV_PIX_FMT_YUV420P;

    stream.time_base = us_rational;

    unwrap_mut(bsf, setup_crop_bsf(params, "h264_metadata", stream));

    return ExternalVideoContext{
        .stream = &stream,
        .bsf    = std::move(bsf),
    };
}

auto Encoder::init_audio_stream_internal(const AudioParamsInternal& params) -> std::optional<InternalAudioContext> {
    auto options = AutoAVDict();
    for(const auto& opt : params.codec.options) {
        av_dict_set(std::inout_ptr(options), opt[0].data(), opt[1].data(), 0);
    }

    unwrap(codec, avcodec_find_encoder_by_name(params.codec.name.data()));
    unwrap_mut(stream, avformat_new_stream(format_context.get(), NULL));
    unwrap_mut(codec_context, avcodec_alloc_context3(&codec));
    codec_context.ch_layout   = params.channel_layout;
    codec_context.sample_fmt  = params.sample_fmt;
    codec_context.sample_rate = params.sample_rate;
    codec_context.time_base   = us_rational;

    if(format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_context.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ensure(avcodec_open2(&codec_context, &codec, std::inout_ptr(options)) >= 0);
    ensure(avcodec_parameters_from_context(stream.codecpar, &codec_context) >= 0);

    return InternalAudioContext{
        .stream        = &stream,
        .codec_context = &codec_context,
    };
}

auto Encoder::init_codecs() -> bool {
    switch(params.video->get_index()) {
    case VideoParams::index_of<VideoParamsInternal>: {
        unwrap_mut(ctx, init_video_stream_internal(params.video->as<VideoParamsInternal>()));
        vctx.emplace<InternalVideoContext>(std::move(ctx));
    } break;
    case VideoParams::index_of<VideoParamsExternal>: {
        unwrap_mut(ctx, init_video_stream_external(params.video->as<VideoParamsExternal>()));
        vctx.emplace<ExternalVideoContext>(std::move(ctx));
    } break;
    }
    switch(params.audio->get_index()) {
    case AudioParams::index_of<AudioParamsInternal>: {
        unwrap_mut(ctx, init_audio_stream_internal(params.audio->as<AudioParamsInternal>()));
        actx.emplace<InternalAudioContext>(std::move(ctx));
    } break;
    }
    if(params.ffmpeg_debug) {
        av_dump_format(format_context.get(), 0, params.output.data(), 1);
    }
    ensure(avio_open(&format_context->pb, params.output.data(), AVIO_FLAG_WRITE) >= 0);

    return true;
}

auto Encoder::init(EncoderParams params_) -> bool {
    params = std::move(params_);

    if(params.ffmpeg_debug) {
        av_log_set_level(AV_LOG_DEBUG);
    }
    avdevice_register_all();

    output_format = av_guess_format(NULL, params.output.data(), NULL);
    ensure(output_format != NULL);

    auto fmt_ctx = (AVFormatContext*)(nullptr);
    ensure(avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, params.output.data()) >= 0);
    format_context.reset(fmt_ctx);

    ensure(init_codecs());

    init_done = true;
    return true;
}

auto Encoder::encode(AVFrame* const frame, AVPacket* packet, bool video) -> bool {
    const auto stream = video ? vctx.as<InternalVideoContext>().stream : actx.as<InternalAudioContext>().stream;
    const auto ctx    = video ? vctx.as<InternalVideoContext>().codec_context : actx.as<InternalAudioContext>().codec_context;
    {
        const auto guard = std::lock_guard(video ? video_encode_lock : audio_encode_lock);
        ensure(avcodec_send_frame(ctx, frame) >= 0);
        if(const auto ret = avcodec_receive_packet(ctx, packet); ret < 0) {
            return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF;
        }
        av_packet_rescale_ts(packet, ctx->time_base, stream->time_base);
    }
    packet->stream_index = stream->index;
    {
        const auto format_guard = std::lock_guard(format_lock);
        ensure(av_interleaved_write_frame(format_context.get(), packet) >= 0);
    }
    return true;
}

auto Encoder::push_frame(AutoAVFrame frame, const int usec) -> bool {
    unwrap_mut(ctx, vctx.get<InternalVideoContext>());

    frame->pts = usec;

    auto filtered = AutoAVFrame(av_frame_alloc());
    ensure(filtered.get() != NULL);
    {
        auto filter_guard = std::lock_guard(filter_lock);
        ensure(av_buffersrc_add_frame_flags(ctx.filter.source_context, frame.get(), 0) >= 0);
        ensure(av_buffersink_get_frame(ctx.filter.sink_context, filtered.get()) >= 0);
    }
    filtered->pict_type = AV_PICTURE_TYPE_NONE;

    auto pkt  = AutoAVPacket(av_packet_alloc());
    pkt->data = NULL;
    pkt->size = 0;
    ensure(encode(filtered.get(), pkt.get(), true));
    return true;
}

auto Encoder::add_frame(std::span<const Plane> planes, const int usec) -> bool {
    ensure(ensure_header({}));
    ensure(planes.size() <= AV_NUM_DATA_POINTERS);

    auto frame = AutoAVFrame(av_frame_alloc());
    ensure(frame.get() != NULL);

    unwrap(params, this->params.video->get<VideoParamsInternal>());
    for(auto i = 0u; i < planes.size(); i += 1) {
        frame->data[i]     = std::bit_cast<uint8_t*>(planes[i].data);
        frame->linesize[i] = planes[i].stride;
    }
    frame->format = params.pix_fmt;
    frame->width  = params.width;
    frame->height = params.height;

    return push_frame(std::move(frame), usec);
}

auto Encoder::mux_packet(AVPacket* const packet, AVStream* const stream, const AVRational src_tb) -> bool {
    av_packet_rescale_ts(packet, src_tb, stream->time_base);
    packet->stream_index    = stream->index;
    const auto format_guard = std::lock_guard(format_lock);
    return av_interleaved_write_frame(format_context.get(), packet) >= 0;
}

auto Encoder::ensure_header(const std::span<const uint8_t> annexb) -> bool {
    if(header_written) {
        return true;
    }

    if(const auto ctx = vctx.get<ExternalVideoContext>()) {
        // externally encoded, need to feed SPS/PPS
        const auto extradata = derive_h264_extradata(annexb);
        ensure(!extradata.empty(), "no SPS/PPS in first H264 packet");

        auto* const par = ctx->stream->codecpar;
        par->extradata  = static_cast<uint8_t*>(av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        ensure(par->extradata != nullptr);
        memcpy(par->extradata, extradata.data(), extradata.size());
        par->extradata_size = int(extradata.size());
    }

    auto dummy = AutoAVDict();
    ensure(avformat_write_header(format_context.get(), std::inout_ptr(dummy)) >= 0);
    header_written = true;

    return true;
}

auto Encoder::add_video_packet(const std::byte* const data, const size_t size, const int64_t pts_us, const bool keyframe) -> bool {
    unwrap(ctx, vctx.get<ExternalVideoContext>());
    auto pkt = AutoAVPacket(av_packet_alloc());
    ensure(av_new_packet(pkt.get(), int(size)) == 0);
    memcpy(pkt->data, data, size);
    pkt->pts = pkt->dts = pts_us;
    if(keyframe) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    const auto guard = std::lock_guard(video_encode_lock);
    if(ctx.bsf != nullptr) {
        ensure(av_bsf_send_packet(ctx.bsf.get(), pkt.get()) >= 0);
        while(true) {
            const auto r = av_bsf_receive_packet(ctx.bsf.get(), pkt.get());
            if(r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
                break;
            }
            ensure(r >= 0);
            ensure(ensure_header(std::span<const uint8_t>{pkt->data, size_t(pkt->size)}));
            ensure(mux_packet(pkt.get(), ctx.stream, us_rational));
            av_packet_unref(pkt.get());
        }
    } else {
        ensure(ensure_header({(uint8_t*)data, size}));
        ensure(mux_packet(pkt.get(), ctx.stream, us_rational));
    }
    return true;
}

auto Encoder::is_header_written() const -> bool {
    return header_written;
}

auto Encoder::get_audio_samples_per_push() const -> size_t {
    return actx.as<InternalAudioContext>().codec_context->frame_size;
}

auto Encoder::add_audio(AVFrame* frame) -> bool {
    auto pkt  = AutoAVPacket(av_packet_alloc());
    pkt->data = NULL;
    pkt->size = 0;
    return encode(frame, pkt.get(), false);
}

auto Encoder::add_audio(const std::span<const std::byte* const> buffers) -> bool {
    unwrap(params, this->params.audio->get<AudioParamsInternal>());
    unwrap(ctx, actx.get<InternalAudioContext>());
    ensure(buffers.size() == size_t(ctx.codec_context->ch_layout.nb_channels));

    auto frame = AutoAVFrame(av_frame_alloc());
    ensure(frame.get() != NULL);

    for(auto i = 0; i < ctx.codec_context->ch_layout.nb_channels; i += 1) {
        frame->data[i] = std::bit_cast<uint8_t*>(buffers[i]);
    }
    frame->sample_rate = params.sample_rate;
    frame->format      = params.sample_fmt;
    frame->ch_layout   = ctx.codec_context->ch_layout;
    frame->nb_samples  = ctx.codec_context->frame_size;

    return add_audio(frame.get());
}

Encoder::~Encoder() {
    if(!init_done) {
        return;
    }

    auto pkt = AutoAVPacket(av_packet_alloc());
    if(vctx.get_index() == VideoContext::index_of<InternalVideoContext>) {
        encode(NULL, pkt.get(), true);
    }
    if(actx.get_index() == AudioContext::index_of<InternalAudioContext>) {
        encode(NULL, pkt.get(), false);
    }

    if(header_written) {
        av_write_trailer(format_context.get());
    }
    if(output_format && (!(output_format->flags & AVFMT_NOFILE))) {
        avio_closep(&format_context->pb);
    }
}
} // namespace ff
