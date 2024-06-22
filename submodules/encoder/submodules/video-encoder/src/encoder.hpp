#pragma once
#include <optional>
#include <span>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
}

#include "common.hpp"
#include "macros/autoptr.hpp"

namespace ff {
declare_autoptr(FormatContext, AVFormatContext, avformat_free_context);
av_declare_autoptr(AVFilterGraph, AVFilterGraph, avfilter_graph_free);
av_declare_autoptr(AVBufferRef, AVBufferRef, av_buffer_unref);

struct Plane {
    const std::byte* data;
    int              stride;
};

struct Codec {
    std::string                             name;
    std::vector<std::array<std::string, 2>> options;
};

struct VideoParams {
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

struct AudioParams {
    Codec           codec;
    AVSampleFormat  sample_fmt;
    int             sample_rate;
    AVChannelLayout channel_layout;
};

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

    struct HWBuffers {
        AutoAVBufferRef device;
        AVBufferRef*    frame_context;
        AutoAVFrame     frame;
    };

    EncoderParams         params;
    const AVOutputFormat* output_format;
    AutoFormatContext     format_context;
    AVStream*             video_stream;
    AVCodecContext*       video_codec_context;
    VideoFilter           video_filter;
    HWBuffers             hw_bufs;
    AVStream*             audio_stream;
    AVCodecContext*       audio_codec_context;
    bool                  init_done = false;
    bool                  use_vaapi;

    std::mutex format_lock;
    std::mutex video_encode_lock;
    std::mutex audio_encode_lock;
    std::mutex filter_lock;

    auto create_video_filter(const VideoParams& params) -> std::optional<VideoFilter>;
    auto init_video_stream(const VideoParams& params) -> bool;
    auto init_audio_stream(const AudioParams& params) -> bool;
    auto init_codecs() -> bool;
    auto encode(AVFrame* frame, AVPacket* packet, bool video) -> bool;
    auto push_frame(AutoAVFrame frame, int usec) -> bool;

  public:
    auto init(EncoderParams params) -> bool;
    auto add_frame(std::span<const Plane> planes, int usec) -> bool;
    auto get_audio_samples_per_push() const -> size_t;
    auto add_audio(AVFrame* frame) -> bool;
    auto add_audio(std::span<const std::byte* const> buffers) -> bool;

    ~Encoder();
};
} // namespace ff
