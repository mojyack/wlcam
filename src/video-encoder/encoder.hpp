#pragma once
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

#include "../macros/autoptr.hpp"
#include "../util/variant.hpp"
#include "common.hpp"

namespace ff {
declare_autoptr(FormatContext, AVFormatContext, avformat_free_context);
av_declare_autoptr(AVFilterGraph, AVFilterGraph, avfilter_graph_free);
av_declare_autoptr(AVBufferRef, AVBufferRef, av_buffer_unref);
av_declare_autoptr(AVBSFContext, AVBSFContext, av_bsf_free);

struct Plane {
    const std::byte* data;
    int              stride;
};

struct Codec {
    std::string                             name;
    std::vector<std::array<std::string, 2>> options;
};

// parameters when using internal(ffmpeg) encoder
struct VideoParamsInternal {
    Codec         codec;
    AVPixelFormat pix_fmt;
    int           width;
    int           height;
    int           b_frames = 3;
    int           threads  = 0;
    std::string   filter   = "";

    // vaapi
    std::string render_node = ""; // something like /dev/dri/renderD128
};

// parameters when using external(e.g. venus) encoder
struct VideoParamsExternal {
    int real_width;
    int real_height;
    int coded_width;
    int coded_height;
};

using VideoParams = Variant<VideoParamsInternal, VideoParamsExternal>;

struct AudioParamsInternal {
    Codec           codec;
    AVSampleFormat  sample_fmt;
    int             sample_rate;
    AVChannelLayout channel_layout;
};

using AudioParams = Variant<AudioParamsInternal>;

struct EncoderParams {
    std::string output;

    std::optional<VideoParams> video;
    std::optional<AudioParams> audio;

    bool ffmpeg_debug = false;
};

class Encoder {
  private:
    struct VideoFilter {
        AutoAVFilterGraph graph;
        AVFilterContext*  source_context;
        AVFilterContext*  sink_context;
        AVFilterContext*  format_context;
    };

    struct InternalVideoContext {
        AVStream*       stream;
        AVCodecContext* codec_context;
        VideoFilter     filter;

        // vaapi only
        struct HWBuffers {
            AutoAVBufferRef device;
            AVBufferRef*    frame_context;
            AutoAVFrame     frame;
        };

        HWBuffers hw_bufs;
    };

    struct ExternalVideoContext {
        AVStream*        stream;
        AutoAVBSFContext bsf;
    };

    using VideoContext = Variant<InternalVideoContext, ExternalVideoContext>;

    struct InternalAudioContext {
        AVStream*       stream;
        AVCodecContext* codec_context;
    };

    using AudioContext = Variant<InternalAudioContext>;

    EncoderParams         params;
    const AVOutputFormat* output_format;
    AutoFormatContext     format_context;
    VideoContext          vctx;
    AudioContext          actx;
    bool                  init_done = false;
    bool                  use_vaapi;
    bool                  header_written = false;

    std::mutex format_lock;
    std::mutex video_encode_lock;
    std::mutex audio_encode_lock;
    std::mutex filter_lock;

    auto create_video_filter(const VideoParamsInternal& params, InternalVideoContext::HWBuffers& hw_bufs, AVCodecContext& codec_context) -> std::optional<VideoFilter>;
    auto setup_crop_bsf(const VideoParamsExternal& params, const char* bsf_name, AVStream& stream) -> std::optional<AutoAVBSFContext>;
    auto init_video_stream_internal(const VideoParamsInternal& params) -> std::optional<InternalVideoContext>;
    auto init_video_stream_external(const VideoParamsExternal& params) -> std::optional<ExternalVideoContext>;
    auto init_audio_stream_internal(const AudioParamsInternal& params) -> std::optional<InternalAudioContext>;
    auto init_codecs() -> bool;
    auto encode(AVFrame* frame, AVPacket* packet, bool video) -> bool;
    auto push_frame(AutoAVFrame frame, int usec) -> bool;

    auto mux_packet(AVPacket* packet, AVStream* stream, AVRational src_tb) -> bool;
    auto ensure_header(std::span<const uint8_t> annexb) -> bool;

  public:
    auto init(EncoderParams params) -> bool;
    auto add_frame(std::span<const Plane> planes, int usec) -> bool;
    auto add_video_packet(const std::byte* data, size_t size, int64_t pts_us, bool keyframe) -> bool;
    auto is_header_written() const -> bool;
    auto get_audio_samples_per_push() const -> size_t;
    auto add_audio(AVFrame* frame) -> bool;
    auto add_audio(std::span<const std::byte* const> buffers) -> bool;

    ~Encoder();
};
} // namespace ff
