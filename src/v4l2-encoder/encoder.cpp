#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/videodev2.h>

#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <libdrm/drm_fourcc.h>

#include "../macros/unwrap.hpp"
#include "encoder.hpp"

namespace ff {
namespace {
constexpr char pack_fragment_shader_source[] = {
#embed "frag.glsl"
    ,
    '\0',
};
constexpr char vertex_shader_source[] = {
#embed "vert.glsl"
    ,
    '\0',
};

auto p_eglCreateImageKHR            = PFNEGLCREATEIMAGEKHRPROC(NULL);
auto p_eglDestroyImageKHR           = PFNEGLDESTROYIMAGEKHRPROC();
auto p_glEGLImageTargetTexture2DOES = PFNGLEGLIMAGETARGETTEXTURE2DOESPROC();
auto p_eglCreateSyncKHR             = PFNEGLCREATESYNCKHRPROC();
auto p_eglClientWaitSyncKHR         = PFNEGLCLIENTWAITSYNCKHRPROC();
auto p_eglDestroySyncKHR            = PFNEGLDESTROYSYNCKHRPROC();

auto ensure_api_entries() -> bool {
    if(p_eglCreateImageKHR != NULL) {
        return true;
    }

    p_eglCreateImageKHR            = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    p_eglDestroyImageKHR           = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    p_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    p_eglCreateSyncKHR             = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    p_eglClientWaitSyncKHR         = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
    p_eglDestroySyncKHR            = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    ensure(p_eglCreateImageKHR && p_eglDestroyImageKHR && p_glEGLImageTargetTexture2DOES);
    return true;
}

auto compile(const GLenum type, const char* const src) -> std::optional<GLuint> {
    const auto s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    auto ok = GLint(0);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        auto log = std::array<char, 2048>();
        glGetShaderInfoLog(s, log.size(), nullptr, log.data());
        bail("failed to compile shader: {}", log.data());
    }
    return s;
}

auto xioctl(const int fd, const unsigned long req, void* const arg) -> int {
    auto r = int();
    do {
        r = ioctl(fd, req, arg);
    } while(r < 0 && errno == EINTR);
    return r;
}
} // namespace

auto V4L2H264Encoder::reclaim_output() -> void {
loop:
    auto pl     = v4l2_plane();
    auto db     = v4l2_buffer();
    db.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    db.memory   = V4L2_MEMORY_DMABUF;
    db.length   = 1;
    db.m.planes = &pl;
    if(xioctl(vfd, VIDIOC_DQBUF, &db) < 0) {
        return;
    }
    out_bufs[db.index].state = OutBuf::State::Free;
    goto loop;
}

auto V4L2H264Encoder::drain_capture(const PacketCallback& on_packet) -> void {
loop:
    auto pl     = v4l2_plane();
    auto db     = v4l2_buffer();
    db.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    db.memory   = V4L2_MEMORY_MMAP;
    db.length   = 1;
    db.m.planes = &pl;
    if(xioctl(vfd, VIDIOC_DQBUF, &db) < 0) {
        return;
    }
    if(pl.bytesused > 0) {
        on_packet(Packet{
            .data     = static_cast<const std::byte*>(cap_ptr[db.index]),
            .size     = pl.bytesused,
            .pts_us   = int64_t(db.timestamp.tv_sec) * 1000000 + db.timestamp.tv_usec,
            .keyframe = (db.flags & V4L2_BUF_FLAG_KEYFRAME) != 0,
        });
    }
    if(db.flags & V4L2_BUF_FLAG_LAST) {
        return;
    }

    auto qp     = v4l2_plane();
    auto qb     = v4l2_buffer();
    qb.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    qb.memory   = V4L2_MEMORY_MMAP;
    qb.index    = db.index;
    qb.length   = 1;
    qb.m.planes = &qp;
    xioctl(vfd, VIDIOC_QBUF, &qb);

    goto loop;
}

auto V4L2H264Encoder::render_into(OutBuf& b, const GLuint src_rgba) -> void {
    static const GLfloat verts[] = {-1, -1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1, -1, 1, 0, 1};
    static const GLuint  elems[] = {0, 1, 2, 2, 3, 0};

    glUseProgram(pack_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_rgba);
    glUniform1i(loc_src, 0);
    glUniform2i(loc_srcsize, width, height);

    const auto pos = glGetAttribLocation(pack_prog, "position");
    const auto tc  = glGetAttribLocation(pack_prog, "texcoord");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, verts);
    glEnableVertexAttribArray(pos);
    glVertexAttribPointer(tc, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, verts + 2);
    glEnableVertexAttribArray(tc);

    glBindFramebuffer(GL_FRAMEBUFFER, b.fbo_y);
    glViewport(0, 0, coded_w, coded_h);
    glUniform2i(loc_dstsize, coded_w, coded_h);
    glUniform1i(loc_plane, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, elems);

    glBindFramebuffer(GL_FRAMEBUFFER, b.fbo_uv);
    glViewport(0, 0, coded_w / 2, coded_h / 2);
    glUniform2i(loc_dstsize, coded_w / 2, coded_h / 2);
    glUniform1i(loc_plane, 1);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, elems);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(have_fence) {
        glFlush();
        b.fence = p_eglCreateSyncKHR(egl_dpy, EGL_SYNC_FENCE_KHR, nullptr);
        if(b.fence == EGL_NO_SYNC_KHR) {
            b.fence = nullptr;
            glFinish();
        }
    } else {
        glFinish();
    }
}

auto V4L2H264Encoder::qbuf_output(const int idx) -> bool {
    auto& buf = out_bufs[idx];

    auto op      = v4l2_plane();
    op.length    = sizeimage;
    op.bytesused = sizeimage;
    op.m.fd      = buf.fd;

    auto ob              = v4l2_buffer();
    ob.type              = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ob.memory            = V4L2_MEMORY_DMABUF;
    ob.index             = idx;
    ob.length            = 1;
    ob.m.planes          = &op;
    ob.timestamp.tv_sec  = buf.pts_us / 1000000;
    ob.timestamp.tv_usec = buf.pts_us % 1000000;

    ensure(xioctl(vfd, VIDIOC_QBUF, &ob) == 0, "QBUF out: {}", strerror(errno));
    buf.state = OutBuf::State::Queued;

    return true;
}

auto V4L2H264Encoder::flush_pending(const bool block) -> void {
    while(!pending.empty()) {
        const auto idx = pending.front();
        auto&      buf = out_bufs[idx];
        if(buf.fence != nullptr) {
            const auto timeout = block ? EGL_FOREVER_KHR : EGLTimeKHR(0);
            const auto r       = p_eglClientWaitSyncKHR(egl_dpy, buf.fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, timeout);
            if(r == EGL_TIMEOUT_EXPIRED_KHR) {
                return; // front not ready yet
            }
            p_eglDestroySyncKHR(egl_dpy, buf.fence);
            buf.fence = nullptr;
        }
        pending.erase(pending.begin());
        qbuf_output(idx);
    }
}

auto V4L2H264Encoder::init(const char* const venus_node, const int width_, const int height_, const int bitrate, const int fps) -> bool {
    width  = width_;
    height = height_;
    ensure((width % 2) == 0 && (height % 2) == 0);

    egl_dpy = eglGetCurrentDisplay();
    ensure(egl_dpy != EGL_NO_DISPLAY);

    ensure(ensure_api_entries());

    const auto egl_exts = eglQueryString(egl_dpy, EGL_EXTENSIONS);

    have_fence = egl_exts != nullptr &&
                 strstr(egl_exts, "EGL_KHR_fence_sync") != nullptr &&
                 p_eglCreateSyncKHR != nullptr &&
                 p_eglClientWaitSyncKHR != nullptr &&
                 p_eglDestroySyncKHR != nullptr;
    if(!have_fence) {
        WARN("fence extension not supported");
    }

    drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    ensure(drm_fd >= 0, "open render node: {}", strerror(errno));
    gbm = gbm_create_device(drm_fd);
    ensure(gbm != nullptr);
    vfd = open(venus_node, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    ensure(vfd >= 0, "no Venus H.264 encoder node found");

    // output (nv12 to encoder)
    auto ofmt                   = v4l2_format();
    ofmt.type                   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ofmt.fmt.pix_mp.width       = width;
    ofmt.fmt.pix_mp.height      = height;
    ofmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    ofmt.fmt.pix_mp.num_planes  = 1;
    ofmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
    ensure(xioctl(vfd, VIDIOC_S_FMT, &ofmt) == 0, "S_FMT out: {}", strerror(errno));
    coded_w   = ofmt.fmt.pix_mp.width;
    coded_h   = ofmt.fmt.pix_mp.height;
    ystride   = ofmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    sizeimage = ofmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    uv_off    = ystride * coded_h;

    // capture (h264)
    auto cfmt                              = v4l2_format();
    cfmt.type                              = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    cfmt.fmt.pix_mp.width                  = width;
    cfmt.fmt.pix_mp.height                 = height;
    cfmt.fmt.pix_mp.pixelformat            = V4L2_PIX_FMT_H264;
    cfmt.fmt.pix_mp.num_planes             = 1;
    cfmt.fmt.pix_mp.plane_fmt[0].sizeimage = 2 * 1024 * 1024;
    ensure(xioctl(vfd, VIDIOC_S_FMT, &cfmt) == 0, "S_FMT cap: {}", strerror(errno));

    auto parm                                 = v4l2_streamparm();
    parm.type                                 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    parm.parm.output.timeperframe.numerator   = 1;
    parm.parm.output.timeperframe.denominator = fps;
    ensure(xioctl(vfd, VIDIOC_S_PARM, &parm) == 0, "S_PARM out: {}", strerror(errno));

    // controls (best-effort)
    for(const auto ctrl : {
            v4l2_control{V4L2_CID_MPEG_VIDEO_BITRATE, bitrate},
            v4l2_control{V4L2_CID_MPEG_VIDEO_GOP_SIZE, fps},
            v4l2_control{V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_HIGH},
        }) {
        ioctl(vfd, VIDIOC_S_CTRL, &ctrl);
    }

    // output buffers = imported dmabufs we render into
    auto orb   = v4l2_requestbuffers();
    orb.count  = num_out;
    orb.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    orb.memory = V4L2_MEMORY_DMABUF;
    ensure(xioctl(vfd, VIDIOC_REQBUFS, &orb) == 0, "REQBUFS out dmabuf: {}", strerror(errno));

    // pack shader
    pack_prog = glCreateProgram();
    unwrap(vertex_shader, compile(GL_VERTEX_SHADER, vertex_shader_source));
    unwrap(fragment_shader, compile(GL_FRAGMENT_SHADER, pack_fragment_shader_source));
    glAttachShader(pack_prog, vertex_shader);
    glAttachShader(pack_prog, fragment_shader);
    glLinkProgram(pack_prog);
    loc_src     = glGetUniformLocation(pack_prog, "src");
    loc_srcsize = glGetUniformLocation(pack_prog, "src_size");
    loc_dstsize = glGetUniformLocation(pack_prog, "dst_size");
    loc_plane   = glGetUniformLocation(pack_prog, "plane");

    out_bufs.resize(num_out);
    const auto rows = (sizeimage + ystride - 1) / ystride + 1;
    for(auto& buf : out_bufs) {
        buf.bo = gbm_bo_create(gbm, ystride, rows, GBM_FORMAT_R8, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
        ensure(buf.bo != nullptr, "gbm_bo_create");
        buf.fd = gbm_bo_get_fd(buf.bo);

        const EGLint attrs_y[] = {EGL_WIDTH, coded_w,
                                  EGL_HEIGHT, coded_h,
                                  EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_R8,
                                  EGL_DMA_BUF_PLANE0_FD_EXT, buf.fd,
                                  EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                                  EGL_DMA_BUF_PLANE0_PITCH_EXT, ystride,
                                  EGL_NONE};

        const EGLint attrs_uv[] = {EGL_WIDTH, coded_w / 2,
                                   EGL_HEIGHT, coded_h / 2,
                                   EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_GR88,
                                   EGL_DMA_BUF_PLANE0_FD_EXT, buf.fd,
                                   EGL_DMA_BUF_PLANE0_OFFSET_EXT, uv_off,
                                   EGL_DMA_BUF_PLANE0_PITCH_EXT, ystride,
                                   EGL_NONE};

        buf.img_y = p_eglCreateImageKHR(egl_dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs_y);
        ensure(buf.img_y != EGL_NO_IMAGE_KHR);
        buf.img_uv = p_eglCreateImageKHR(egl_dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs_uv);
        ensure(buf.img_y != EGL_NO_IMAGE_KHR && buf.img_uv != EGL_NO_IMAGE_KHR, "eglCreateImage plane");

        glGenTextures(1, &buf.tex_y);
        glBindTexture(GL_TEXTURE_2D, buf.tex_y);
        p_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf.img_y);
        glGenTextures(1, &buf.tex_uv);
        glBindTexture(GL_TEXTURE_2D, buf.tex_uv);
        p_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf.img_uv);
        glGenFramebuffers(1, &buf.fbo_y);
        glBindFramebuffer(GL_FRAMEBUFFER, buf.fbo_y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buf.tex_y, 0);
        ensure(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Y fbo incomplete");
        glGenFramebuffers(1, &buf.fbo_uv);
        glBindFramebuffer(GL_FRAMEBUFFER, buf.fbo_uv);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buf.tex_uv, 0);
        ensure(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "UV fbo incomplete");
    }

    // capture buffers = mmap, queued up front
    auto crb   = v4l2_requestbuffers();
    crb.count  = num_cap;
    crb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    crb.memory = V4L2_MEMORY_MMAP;
    ensure(xioctl(vfd, VIDIOC_REQBUFS, &crb) == 0, "REQBUFS cap mmap: {}", strerror(errno));
    cap_ptr.resize(crb.count);
    cap_len.resize(crb.count);
    for(auto i = 0u; i < crb.count; i += 1) {
        auto pl     = v4l2_plane();
        auto qb     = v4l2_buffer();
        qb.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        qb.memory   = V4L2_MEMORY_MMAP;
        qb.index    = i;
        qb.length   = 1;
        qb.m.planes = &pl;
        ensure(xioctl(vfd, VIDIOC_QUERYBUF, &qb) == 0, "QUERYBUF cap");
        cap_len[i] = pl.length;
        cap_ptr[i] = mmap(nullptr, pl.length, PROT_READ | PROT_WRITE, MAP_SHARED, vfd, pl.m.mem_offset);
        ensure(cap_ptr[i] != MAP_FAILED, "mmap cap");
        ensure(xioctl(vfd, VIDIOC_QBUF, &qb) == 0, "QBUF cap init");
    }

    auto t = int(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    ensure(xioctl(vfd, VIDIOC_STREAMON, &t) == 0, "STREAMON out");
    t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ensure(xioctl(vfd, VIDIOC_STREAMON, &t) == 0, "STREAMON cap");
    return true;
}

auto V4L2H264Encoder::encode(const GLuint src_rgba, const int64_t pts_us, const PacketCallback& on_packet) -> bool {
    const auto pick_free = [&]() {
        for(auto i = 0; i < num_out; i += 1) {
            if(out_bufs[i].state == OutBuf::State::Free) {
                return i;
            }
        }
        return -1;
    };

    auto idx = -1;
    for(;;) {
        reclaim_output();
        flush_pending(false); // QBUF anything the GPU has finished
        idx = pick_free();
        if(idx >= 0) {
            break;
        }
        // no free buffer: wait for the encoder to consume a queued one,
        // or (if everything is still mid-render) block on the oldest fence to free it up.
        auto any_queued = false;
        for(const auto& buf : out_bufs) {
            any_queued |= buf.state == OutBuf::State::Queued;
        }
        if(any_queued) {
            auto pfd = pollfd{.fd = vfd, .events = POLLOUT, .revents = 0};
            poll(&pfd, 1, 200);
        } else {
            flush_pending(true);
        }
    }

    auto& buf  = out_bufs[idx];
    buf.pts_us = pts_us;
    render_into(buf, src_rgba); // sets buf.fence when fences are available
    if(buf.fence != nullptr) {
        buf.state = OutBuf::State::Pending;
        pending.push_back(idx);
    } else {
        qbuf_output(idx); // glFinish fallback: writes already landed
    }

    drain_capture(on_packet);
    return true;
}

auto V4L2H264Encoder::drain(const PacketCallback& on_packet) -> bool {
    flush_pending(true); // QBUF every rendered-but-pending frame before stopping

    auto ec = v4l2_encoder_cmd();
    ec.cmd  = V4L2_ENC_CMD_STOP;
    ioctl(vfd, VIDIOC_ENCODER_CMD, &ec);

    for(auto i = 0; i < 64; i += 1) {
        auto pfd = pollfd{.fd = vfd, .events = POLLIN, .revents = 0};
        if(poll(&pfd, 1, 200) <= 0) {
            break;
        }
        drain_capture(on_packet);
    }
    return true;
}

auto V4L2H264Encoder::coded_width() const -> int {
    return coded_w;
}

auto V4L2H264Encoder::coded_height() const -> int {
    return coded_h;
}

V4L2H264Encoder::~V4L2H264Encoder() {
    if(vfd >= 0) {
        auto t = int(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
        xioctl(vfd, VIDIOC_STREAMOFF, &t);
        t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        xioctl(vfd, VIDIOC_STREAMOFF, &t);
    }
    for(auto& buf : out_bufs) {
        if(buf.fence) p_eglDestroySyncKHR(egl_dpy, buf.fence);
        if(buf.fbo_y) glDeleteFramebuffers(1, &buf.fbo_y);
        if(buf.fbo_uv) glDeleteFramebuffers(1, &buf.fbo_uv);
        if(buf.tex_y) glDeleteTextures(1, &buf.tex_y);
        if(buf.tex_uv) glDeleteTextures(1, &buf.tex_uv);
        if(buf.img_y) p_eglDestroyImageKHR(egl_dpy, buf.img_y);
        if(buf.img_uv) p_eglDestroyImageKHR(egl_dpy, buf.img_uv);
        if(buf.fd >= 0) close(buf.fd);
        if(buf.bo) gbm_bo_destroy(buf.bo);
    }
    if(pack_prog) glDeleteProgram(pack_prog);
    for(auto i = 0uz; i < cap_ptr.size(); i += 1) {
        if(cap_ptr[i] && cap_ptr[i] != MAP_FAILED) munmap(cap_ptr[i], cap_len[i]);
    }
    if(vfd >= 0) close(vfd);
    if(gbm) gbm_device_destroy(gbm);
    if(drm_fd >= 0) close(drm_fd);
}
} // namespace ff
