#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include <gbm.h>

#include <GL/gl.h>

namespace ff {
class V4L2H264Encoder {
  public:
    struct Packet {
        const std::byte* data;
        size_t           size;
        int64_t          pts_us;
        bool             keyframe;
    };
    using PacketCallback = std::function<void(const Packet&)>;

  private:
    struct OutBuf {
        // free -> pending (rendered, GPU fence outstanding) -> queued (in V4L2)
        enum class State {
            Free,
            Pending,
            Queued,
        };

        gbm_bo* bo     = nullptr;
        int     fd     = -1;
        void*   img_y  = nullptr; // EGLImageKHR
        void*   img_uv = nullptr;
        GLuint  tex_y  = 0;
        GLuint  tex_uv = 0;
        GLuint  fbo_y  = 0;
        GLuint  fbo_uv = 0;
        State   state  = State::Free;
        void*   fence  = nullptr; // EGLSyncKHR while pending (GPU render completion)
        int64_t pts_us = 0;
    };

    int         vfd     = -1;
    void*       egl_dpy = nullptr; // EGLDisplay
    int         drm_fd  = -1;
    gbm_device* gbm     = nullptr;

    int width     = 0;
    int height    = 0;
    int coded_w   = 0;
    int coded_h   = 0;
    int ystride   = 0;
    int sizeimage = 0;
    int uv_off    = 0;

    static constexpr int num_out = 6;
    static constexpr int num_cap = 4;
    std::vector<OutBuf>  out_bufs;
    std::vector<int>     pending; // rendered buffers awaiting fence->QBUF, in order
    bool                 have_fence = false;
    std::vector<void*>   cap_ptr;
    std::vector<size_t>  cap_len;

    GLuint pack_prog   = 0;
    GLint  loc_src     = -1;
    GLint  loc_srcsize = -1;
    GLint  loc_dstsize = -1;
    GLint  loc_plane   = -1;

    auto reclaim_output() -> void;
    auto drain_capture(const PacketCallback& on_packet) -> void;
    auto render_into(OutBuf& b, GLuint src_rgba) -> void;
    auto qbuf_output(int idx) -> bool;
    auto flush_pending(bool block) -> void;

  public:
    auto init(const char* venus_node, int width, int height, int bitrate, int fps) -> bool;
    auto encode(GLuint src_rgba, int64_t pts_us, const PacketCallback& on_packet) -> bool;
    auto drain(const PacketCallback& on_packet) -> bool;
    auto coded_width() const -> int;
    auto coded_height() const -> int;

    ~V4L2H264Encoder();
};
} // namespace ff
