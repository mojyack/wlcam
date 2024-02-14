#pragma once
#include <filesystem>

#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

namespace iocd {
inline auto fdpath(const int fd) -> std::string {
    const auto fd_path = std::string("/proc/self/fd/") + std::to_string(fd);
    return std::filesystem::read_symlink(fd_path);
}

struct Printer {
    static constexpr auto indent_width = 2;

    struct AutoIndent {
        Printer* self;

        ~AutoIndent() {
            self->indent -= indent_width;
        }
    };

    FILE* output;
    int   indent;

    auto clone() -> Printer {
        return Printer{output, indent + indent_width};
    }

    auto do_indent() -> AutoIndent {
        indent += indent_width;
        return {this};
    }

    template <class... Args>
    auto operator()(const char* const fmt, Args&&... args) -> void {
        for(auto i = 0; i < indent; i += 1) {
            fprintf(output, " ");
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
        fprintf(output, fmt, args...);
#pragma clang diagnostic pop
    }
};

#define CASE(v) \
    case v:     \
        return #v

inline auto buftype_str(const v4l2_buf_type type) -> const char* {
    switch(type) {
        CASE(V4L2_BUF_TYPE_SDR_CAPTURE);
        CASE(V4L2_BUF_TYPE_SDR_OUTPUT);
        CASE(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
        CASE(V4L2_BUF_TYPE_SLICED_VBI_OUTPUT);
        CASE(V4L2_BUF_TYPE_VBI_CAPTURE);
        CASE(V4L2_BUF_TYPE_VBI_OUTPUT);
        CASE(V4L2_BUF_TYPE_VIDEO_CAPTURE);
        CASE(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
        CASE(V4L2_BUF_TYPE_VIDEO_OUTPUT);
        CASE(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
        CASE(V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY);
        CASE(V4L2_BUF_TYPE_VIDEO_OVERLAY);
        CASE(V4L2_BUF_TYPE_META_CAPTURE);
        CASE(V4L2_BUF_TYPE_META_OUTPUT);
    default:
        return "unknown";
    }
}

inline auto field_str(const v4l2_field type) -> const char* {
    switch(type) {
        CASE(V4L2_FIELD_ANY);
        CASE(V4L2_FIELD_NONE);
        CASE(V4L2_FIELD_TOP);
        CASE(V4L2_FIELD_BOTTOM);
        CASE(V4L2_FIELD_INTERLACED);
        CASE(V4L2_FIELD_SEQ_TB);
        CASE(V4L2_FIELD_SEQ_BT);
        CASE(V4L2_FIELD_ALTERNATE);
        CASE(V4L2_FIELD_INTERLACED_TB);
        CASE(V4L2_FIELD_INTERLACED_BT);
    default:
        return "unknown";
    }
}

inline auto colorspace_str(const v4l2_colorspace type) -> const char* {
    switch(type) {
        CASE(V4L2_COLORSPACE_DEFAULT);
        CASE(V4L2_COLORSPACE_SMPTE170M);
        CASE(V4L2_COLORSPACE_REC709);
        CASE(V4L2_COLORSPACE_SRGB);
        CASE(V4L2_COLORSPACE_ADOBERGB);
        CASE(V4L2_COLORSPACE_BT2020);
        CASE(V4L2_COLORSPACE_DCI_P3);
        CASE(V4L2_COLORSPACE_SMPTE240M);
        CASE(V4L2_COLORSPACE_470_SYSTEM_M);
        CASE(V4L2_COLORSPACE_470_SYSTEM_BG);
        CASE(V4L2_COLORSPACE_JPEG);
        CASE(V4L2_COLORSPACE_RAW);
    default:
        return "unknown";
    }
}

inline auto framesizetype_str(const v4l2_frmsizetypes type) -> const char* {
    switch(type) {
        CASE(V4L2_FRMSIZE_TYPE_CONTINUOUS);
        CASE(V4L2_FRMSIZE_TYPE_DISCRETE);
        CASE(V4L2_FRMSIZE_TYPE_STEPWISE);
    default:
        return "unknown";
    }
}

inline auto memorytype_str(const v4l2_memory type) -> const char* {
    switch(type) {
        CASE(V4L2_MEMORY_MMAP);
        CASE(V4L2_MEMORY_USERPTR);
        CASE(V4L2_MEMORY_OVERLAY);
        CASE(V4L2_MEMORY_DMABUF);
    default:
        return "unknown";
    }
}

inline auto eventtype_str(const uint16_t type) -> const char* {
    switch(type) {
        CASE(V4L2_EVENT_ALL);
        CASE(V4L2_EVENT_VSYNC);
        CASE(V4L2_EVENT_EOS);
        CASE(V4L2_EVENT_CTRL);
        CASE(V4L2_EVENT_FRAME_SYNC);
        CASE(V4L2_EVENT_SOURCE_CHANGE);
        CASE(V4L2_EVENT_MOTION_DET);
    default:
        return "unknown";
    }
}

#undef CASE

#define APPEND(v) \
    if(flag & v) ret += #v "|"

inline auto bufferflag_str(const uint32_t flag) -> std::string {
    auto ret = std::string();

    APPEND(V4L2_BUF_FLAG_MAPPED);
    APPEND(V4L2_BUF_FLAG_QUEUED);
    APPEND(V4L2_BUF_FLAG_DONE);
    APPEND(V4L2_BUF_FLAG_ERROR);
    APPEND(V4L2_BUF_FLAG_KEYFRAME);
    APPEND(V4L2_BUF_FLAG_BFRAME);
    APPEND(V4L2_BUF_FLAG_TIMECODE);
    APPEND(V4L2_BUF_FLAG_PREPARED);
    APPEND(V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
    APPEND(V4L2_BUF_FLAG_NO_CACHE_CLEAN);
    APPEND(V4L2_BUF_FLAG_LAST);
    APPEND(V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN);
    APPEND(V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC);
    APPEND(V4L2_BUF_FLAG_TIMESTAMP_COPY);
    APPEND(V4L2_BUF_FLAG_TSTAMP_SRC_MASK);
    APPEND(V4L2_BUF_FLAG_TSTAMP_SRC_EOF);
    APPEND(V4L2_BUF_FLAG_TSTAMP_SRC_SOE);

    if(!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}

inline auto openflag_str(const uint32_t flag) -> std::string {
    auto ret = std::string();

    APPEND(O_RDONLY);
    APPEND(O_WRONLY);
    APPEND(O_RDWR);
    APPEND(O_CLOEXEC);

    if(!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}

inline auto padflag_str(const uint32_t flag) -> std::string {
    auto ret = std::string();

    APPEND(MEDIA_PAD_FL_SINK);
    APPEND(MEDIA_PAD_FL_SOURCE);
    APPEND(MEDIA_PAD_FL_MUST_CONNECT);

    if(!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}

inline auto linkflag_str(const uint32_t flag) -> std::string {
    auto ret = std::string();

    APPEND(MEDIA_LNK_FL_ENABLED);
    APPEND(MEDIA_LNK_FL_IMMUTABLE);
    APPEND(MEDIA_LNK_FL_DYNAMIC);
    APPEND(MEDIA_LNK_FL_DATA_LINK);
    APPEND(MEDIA_LNK_FL_INTERFACE_LINK);

    if(!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}

inline auto eventsubflags_str(const uint32_t flag) -> std::string {
    auto ret = std::string();

    APPEND(V4L2_EVENT_SUB_FL_SEND_INITIAL);
    APPEND(V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK);

    if(!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}

#undef APPEND

inline auto print_capability(Printer print, const int cap) -> void {
    print("V4L2_CAP_VIDEO_CAPTURE        %d\n", (bool)(cap & V4L2_CAP_VIDEO_CAPTURE));
    print("V4L2_CAP_VIDEO_OUTPUT         %d\n", (bool)(cap & V4L2_CAP_VIDEO_OUTPUT));
    print("V4L2_CAP_VIDEO_OVERLAY        %d\n", (bool)(cap & V4L2_CAP_VIDEO_OVERLAY));
    print("V4L2_CAP_VBI_CAPTURE          %d\n", (bool)(cap & V4L2_CAP_VBI_CAPTURE));
    print("V4L2_CAP_VBI_OUTPUT           %d\n", (bool)(cap & V4L2_CAP_VBI_OUTPUT));
    print("V4L2_CAP_SLICED_VBI_CAPTURE   %d\n", (bool)(cap & V4L2_CAP_SLICED_VBI_CAPTURE));
    print("V4L2_CAP_SLICED_VBI_OUTPUT    %d\n", (bool)(cap & V4L2_CAP_SLICED_VBI_OUTPUT));
    print("V4L2_CAP_RDS_CAPTURE          %d\n", (bool)(cap & V4L2_CAP_RDS_CAPTURE));
    print("V4L2_CAP_VIDEO_OUTPUT_OVERLAY %d\n", (bool)(cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY));
    print("V4L2_CAP_HW_FREQ_SEEK         %d\n", (bool)(cap & V4L2_CAP_HW_FREQ_SEEK));
    print("V4L2_CAP_RDS_OUTPUT           %d\n", (bool)(cap & V4L2_CAP_RDS_OUTPUT));
    print("V4L2_CAP_VIDEO_CAPTURE_MPLANE %d\n", (bool)(cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE));
    print("V4L2_CAP_VIDEO_OUTPUT_MPLANE  %d\n", (bool)(cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE));
    print("V4L2_CAP_VIDEO_M2M_MPLANE     %d\n", (bool)(cap & V4L2_CAP_VIDEO_M2M_MPLANE));
    print("V4L2_CAP_VIDEO_M2M            %d\n", (bool)(cap & V4L2_CAP_VIDEO_M2M));
    print("V4L2_CAP_TUNER                %d\n", (bool)(cap & V4L2_CAP_TUNER));
    print("V4L2_CAP_AUDIO                %d\n", (bool)(cap & V4L2_CAP_AUDIO));
    print("V4L2_CAP_RADIO                %d\n", (bool)(cap & V4L2_CAP_RADIO));
    print("V4L2_CAP_MODULATOR            %d\n", (bool)(cap & V4L2_CAP_MODULATOR));
    print("V4L2_CAP_SDR_CAPTURE          %d\n", (bool)(cap & V4L2_CAP_SDR_CAPTURE));
    print("V4L2_CAP_EXT_PIX_FORMAT       %d\n", (bool)(cap & V4L2_CAP_EXT_PIX_FORMAT));
    print("V4L2_CAP_SDR_OUTPUT           %d\n", (bool)(cap & V4L2_CAP_SDR_OUTPUT));
    print("V4L2_CAP_META_CAPTURE         %d\n", (bool)(cap & V4L2_CAP_META_CAPTURE));
    print("V4L2_CAP_READWRITE            %d\n", (bool)(cap & V4L2_CAP_READWRITE));
    print("V4L2_CAP_STREAMING            %d\n", (bool)(cap & V4L2_CAP_STREAMING));
    print("V4L2_CAP_META_OUTPUT          %d\n", (bool)(cap & V4L2_CAP_META_OUTPUT));
    print("V4L2_CAP_TOUCH                %d\n", (bool)(cap & V4L2_CAP_TOUCH));
    print("V4L2_CAP_IO_MC                %d\n", (bool)(cap & V4L2_CAP_IO_MC));
    print("V4L2_CAP_DEVICE_CAPS          %d\n", (bool)(cap & V4L2_CAP_DEVICE_CAPS));
}

inline auto print(Printer put, const v4l2_capability& argp) -> void {
    put("v4l2_capability {\n");
    {
        auto _ = put.do_indent();
        put("driver:  %s\n", argp.driver);
        put("card:    %s\n", argp.card);
        put("bus:     %s\n", argp.bus_info);
        put("version: %u.%u.%u\n", (argp.version >> 16) & 0xFF, (argp.version >> 8) & 0xFF, argp.version & 0xFF);
        put("caps:\n");
        print_capability(put.clone(), argp.capabilities);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_subdev_capability& argp) -> void {
    put("v4l2_subdev_capability {\n");
    {
        auto _ = put.do_indent();
        put("version: %u.%u.%u\n", (argp.version >> 16) & 0xFF, (argp.version >> 8) & 0xFF, argp.version & 0xFF);
        put("caps:\n");
        print_capability(put.clone(), argp.capabilities);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_pix_format& argp) -> void {
    put("v4l2_pix_format {\n");
    {
        auto _ = put.do_indent();
        put("width:        %u\n", argp.width);
        put("height:       %u\n", argp.height);
        put("format:       %c%c%c%c\n", argp.pixelformat >> 0, argp.pixelformat >> 8, argp.pixelformat >> 16, argp.pixelformat >> 24);
        put("field:        %s\n", field_str(v4l2_field(argp.field)));
        put("bytesperline: %u\n", argp.bytesperline);
        put("sizeimage:    %u\n", argp.sizeimage);
        put("colorspace:   %s\n", colorspace_str(v4l2_colorspace(argp.colorspace)));
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_pix_format_mplane& argp) -> void {
    put("v4l2_pix_format_mplane {\n");
    {
        auto _ = put.do_indent();
        put("width:        %u\n", argp.width);
        put("height:       %u\n", argp.height);
        put("pixelformat:  %c%c%c%c\n", argp.pixelformat >> 0, argp.pixelformat >> 8, argp.pixelformat >> 16, argp.pixelformat >> 24);
        put("field:        %s\n", field_str(v4l2_field(argp.field)));
        put("colorspace:   %s\n", colorspace_str(v4l2_colorspace(argp.colorspace)));
        put("plane_fmt:\n");
        for(auto i = 0u; i < argp.num_planes; i += 1) {
            auto _ = put.do_indent();
            put("plane_fmt[%d]:\n", i);
            {
                auto _ = put.do_indent();
                put("bytesperline: %u\n", argp.plane_fmt[i].bytesperline);
                put("sizeimage:    %u\n", argp.plane_fmt[i].sizeimage);
            }
        }
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_format& argp) -> void {
    put("v4l2_format {\n");
    {
        auto _ = put.do_indent();
        put("type: %s\n", buftype_str(v4l2_buf_type(argp.type)));
        switch(argp.type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
            put("fmt.pix:\n");
            print(put.clone(), argp.fmt.pix);
            break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
            put("fmt.pix_mp:\n");
            print(put.clone(), argp.fmt.pix_mp);
            break;
        }
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_fmtdesc& argp) -> void {
    put("v4l2_fmtdesc {\n");
    {
        auto _ = put.do_indent();
        put("index:       %u\n", argp.index);
        put("type:        %s\n", buftype_str(v4l2_buf_type(argp.type)));
        put("description: %s\n", argp.description);
        put("format:      %c%c%c%c\n", argp.pixelformat >> 0, argp.pixelformat >> 8, argp.pixelformat >> 16, argp.pixelformat >> 24);
        put("mbus_code:   %u\n", argp.mbus_code);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_frmsize_discrete& argp) -> void {
    put("v4l2_frmsize_discrete {\n");
    {
        auto _ = put.do_indent();
        put("width:  %u\n", argp.width);
        put("height: %u\n", argp.height);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_frmsize_stepwise& argp) -> void {
    put("v4l2_frmsize_stepwise {\n");
    {
        auto _ = put.do_indent();
        put("min_width:   %u\n", argp.min_width);
        put("min_height:  %u\n", argp.min_height);
        put("max_width:   %u\n", argp.max_width);
        put("max_height:  %u\n", argp.max_height);
        put("step_width:  %u\n", argp.step_width);
        put("step_height: %u\n", argp.step_height);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_frmsizeenum& argp) -> void {
    put("v4l2_frmsizeenum {\n");
    {
        auto _ = put.do_indent();
        put("index:       %u\n", argp.index);
        put("format:      %c%c%c%c\n", argp.pixel_format >> 0, argp.pixel_format >> 8, argp.pixel_format >> 16, argp.pixel_format >> 24);
        put("type:        %s\n", framesizetype_str(v4l2_frmsizetypes(argp.type)));
        switch(argp.type) {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            put("discrete:\n");
            print(put.clone(), argp.discrete);
            break;
        case V4L2_FRMSIZE_TYPE_STEPWISE:
            put("stepwise:\n");
            print(put.clone(), argp.stepwise);
            break;
        }
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_subdev_mbus_code_enum& argp) -> void {
    put("v4l2_subdev_mbus_code_enum {\n");
    {
        auto _ = put.do_indent();
        put("pad:   %u\n", argp.pad);
        put("index: %u\n", argp.index);
        put("code:  %x\n", argp.code);
        put("which: %u\n", argp.which);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_subdev_frame_size_enum& argp) -> void {
    put("v4l2_subdev_frame_size_enum {\n");
    {
        auto _ = put.do_indent();
        put("index:      %u\n", argp.index);
        put("pad:        %u\n", argp.pad);
        put("code:       %x\n", argp.code);
        put("min_width:  %u\n", argp.min_width);
        put("min_height: %u\n", argp.min_height);
        put("max_width:  %u\n", argp.max_width);
        put("max_height: %u\n", argp.max_height);
        put("which:      %u\n", argp.which);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_mbus_framefmt& argp) -> void {
    put("v4l2_mbus_framefmt {\n");
    {
        auto _ = put.do_indent();
        put("width:      %u\n", argp.width);
        put("height:     %u\n", argp.height);
        put("code:       %x\n", argp.code);
        put("field:      %s\n", field_str(v4l2_field(argp.field)));
        put("colorspace: %s\n", colorspace_str(v4l2_colorspace(argp.colorspace)));
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_subdev_format& argp) -> void {
    put("v4l2_subdev_format {\n");
    {
        auto _ = put.do_indent();
        put("pad:   %u\n", argp.pad);
        put("which: %u\n", argp.which);
        put("format:\n");
        print(put.clone(), argp.format);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_requestbuffers& argp) -> void {
    put("v4l2_requestbuffers {\n");
    {
        auto _ = put.do_indent();
        put("count:  %u\n", argp.count);
        put("type:   %s\n", buftype_str(v4l2_buf_type(argp.type)));
        put("memory: %s\n", memorytype_str(v4l2_memory(argp.memory)));
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_plane& argp, const v4l2_memory memory) -> void {
    put("v4l2_plane {\n");
    {
        auto _ = put.do_indent();
        put("bytesused:   %u\n", argp.bytesused);
        put("length:      %u\n", argp.length);
        switch(memory) {
        case V4L2_MEMORY_MMAP:
            put("offset:      %u\n", argp.m.mem_offset);
            break;
        case V4L2_MEMORY_USERPTR:
            put("userptr:     %p\n", argp.m.userptr);
            break;
        case V4L2_MEMORY_DMABUF:
            put("fd:          %d\n", argp.m.fd);
            break;
        default:
            break;
        }
        put("data_offset: %u\n", argp.data_offset);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_exportbuffer& argp) -> void {
    put("v4l2_exportbuffer {\n");
    {
        auto _ = put.do_indent();
        put("type:  %s\n", buftype_str(v4l2_buf_type(argp.type)));
        put("index: %u\n", argp.index);
        put("plane: %u\n", argp.plane);
        put("flags: %s\n", openflag_str(argp.flags).c_str());
        put("fd:    %d\n", argp.fd);
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_buffer& argp) -> void {
    put("v4l2_buffer {\n");
    {
        auto _ = put.do_indent();
        put("index:     %u\n", argp.index);
        put("type:      %s\n", buftype_str(v4l2_buf_type(argp.type)));
        put("bytesused: %u\n", argp.bytesused);
        put("flags:     %s\n", bufferflag_str(argp.flags).c_str());
        put("field:     %s\n", field_str(v4l2_field(argp.field)));
        put("memory:    %s\n", memorytype_str(v4l2_memory(argp.memory)));
        if(argp.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || argp.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            put("planes:    %p\n", argp.m.planes);
            for(auto i = 0u; i < argp.length; i += 1) {
                print(put.clone(), argp.m.planes[i], v4l2_memory(argp.memory));
            }
        } else {
            switch(argp.memory) {
            case V4L2_MEMORY_MMAP:
                put("offset:    %u\n", argp.m.offset);
                break;
            case V4L2_MEMORY_USERPTR:
                put("userptr:   %p\n", argp.m.userptr);
                break;
            case V4L2_MEMORY_DMABUF:
                put("fd:        %d\n", argp.m.fd);
                break;
            }
        }
        put("length:    %u\n", argp.length);
    }
    put("}\n");
}

inline auto print(Printer put, const media_pad_desc& argp) -> void {
    put("media_pad_desc {\n");
    {
        auto _ = put.do_indent();
        put("entity: %u\n", argp.entity);
        put("index:  %u\n", argp.index);
        put("flags:  %s\n", padflag_str(argp.flags).c_str());
    }
    put("}\n");
}

inline auto print(Printer put, const media_link_desc& argp) -> void {
    put("media_link_desc {\n");
    {
        auto _ = put.do_indent();
        put("source:\n");
        print(put.clone(), argp.source);
        put("sink:\n");
        print(put.clone(), argp.sink);
        put("flags: %s\n", linkflag_str(argp.flags).c_str());
    }
    put("}\n");
}

inline auto print(Printer put, const v4l2_event_subscription& argp) -> void {
    put("v4l2_event_subscription {\n");
    {
        auto _ = put.do_indent();
        put("type:  %s\n", eventtype_str(argp.type));
        put("id  :  %u\n", argp.id);
        put("flags: %s\n", eventsubflags_str(argp.flags).c_str());
    }
    put("}\n");
}

inline auto ioctl(FILE* output, const int fd, const uint64_t request, const void* const argp) -> int {
    if(output == nullptr) {
        static auto outfile = ::fopen("/tmp/ioctl-dbg", "w");
        output              = outfile;
    }

    auto put = Printer{output, 0};

    put("=== ioctl %s ===\n", fdpath(fd).c_str());
    switch(request) {
    case VIDIOC_QUERYCAP:
        put("VIDIOC_QUERYCAP\n");
        break;
    case VIDIOC_SUBDEV_QUERYCAP:
        put("VIDIOC_SUBDEV_QUERYCAP\n");
        break;
    case VIDIOC_G_FMT:
        put("VIDIOC_G_FMT\n");
        break;
    case VIDIOC_S_FMT:
        put("VIDIOC_S_FMT\n");
        print(put, *(v4l2_format*)(argp));
        break;
    case VIDIOC_TRY_FMT:
        put("VIDIOC_TRY_FMT\n");
        print(put, *(v4l2_format*)(argp));
        break;
    case VIDIOC_ENUM_FMT:
        put("VIDIOC_ENUM_FMT\n");
        print(put, *(v4l2_fmtdesc*)(argp));
        break;
    case VIDIOC_ENUM_FRAMESIZES:
        put("VIDIOC_ENUM_FRAMESIZES\n");
        break;
    case VIDIOC_SUBDEV_ENUM_MBUS_CODE:
        put("VIDIOC_SUBDEV_ENUM_MBUS_CODE\n");
        print(put, *(v4l2_subdev_mbus_code_enum*)(argp));
        break;
    case VIDIOC_SUBDEV_ENUM_FRAME_SIZE:
        put("VIDIOC_SUBDEV_ENUM_FRAME_SIZE\n");
        print(put, *(v4l2_subdev_frame_size_enum*)(argp));
        break;
    case VIDIOC_SUBDEV_G_FMT:
        put("VIDIOC_SUBDEV_G_FMT\n");
        break;
    case VIDIOC_SUBDEV_S_FMT:
        put("VIDIOC_SUBDEV_S_FMT\n");
        print(put, *(v4l2_subdev_format*)(argp));
        break;
    case VIDIOC_REQBUFS:
        put("VIDIOC_REQBUFS\n");
        print(put, *(v4l2_requestbuffers*)(argp));
        break;
    case VIDIOC_QUERYBUF:
        put("VIDIOC_QUERYBUF\n");
        print(put, *(v4l2_buffer*)(argp));
        break;
    case VIDIOC_EXPBUF:
        put("VIDIOC_EXPBUF\n");
        print(put, *(v4l2_exportbuffer*)(argp));
        break;
    case VIDIOC_QBUF:
        put("VIDIOC_QBUF\n");
        print(put, *(v4l2_buffer*)(argp));
        break;
    case VIDIOC_DQBUF:
        put("VIDIOC_DQBUF\n");
        print(put, *(v4l2_buffer*)(argp));
        break;
    case VIDIOC_STREAMON:
        put("VIDIOC_STREAMON %s\n", buftype_str(*(v4l2_buf_type*)(argp)));
        break;
    case VIDIOC_STREAMOFF:
        put("VIDIOC_STREAMOFF %s\n", buftype_str(*(v4l2_buf_type*)(argp)));
        break;
    case MEDIA_IOC_SETUP_LINK:
        put("MEDIA_IOC_SETUP_LINK\n");
        print(put, *(media_link_desc*)(argp));
        break;
    case VIDIOC_SUBSCRIBE_EVENT:
        put("VIDIOC_SUBSCRIBE_EVENT\n");
        print(put, *(v4l2_event_subscription*)(argp));
        break;
    case VIDIOC_UNSUBSCRIBE_EVENT:
        put("VIDIOC_UNSUBSCRIBE_EVENT\n");
        print(put, *(v4l2_event_subscription*)(argp));
        break;

    // stub
    case VIDIOC_QUERY_EXT_CTRL:
        put("VIDIOC_QUERY_EXT_CTRL\n");
        break;
    case VIDIOC_QUERYMENU:
        put("VIDIOC_QUERYMENU\n");
        break;
    case VIDIOC_G_EXT_CTRLS:
        put("VIDIOC_S_EXT_CTRLS\n");
        break;
    case VIDIOC_S_EXT_CTRLS:
        put("VIDIOC_S_EXT_CTRLS\n");
        break;
    case VIDIOC_SUBDEV_G_SELECTION:
        put("VIDIOC_SUBDEV_G_SELECTION\n");
        break;
    case VIDIOC_SUBDEV_S_SELECTION:
        put("VIDIOC_SUBDEV_S_SELECTION\n");
        break;
    case VIDIOC_DQEVENT:
        put("VIDIOC_DQEVENT\n");
        break;
    case V4L2_SEL_TGT_CROP_BOUNDS:
        put("V4L2_SEL_TGT_CROP_BOUNDS\n");
        break;

    default:
        put("unknown(%lu)\n", request);
        break;
    }

    const auto r = ::ioctl(fd, request, argp);
    const auto e = errno;
    put("  => %d (errno=%d)\n", r, e);

    switch(request) {
    case VIDIOC_QUERYCAP:
        print(put, *(v4l2_capability*)(argp));
        break;
    case VIDIOC_SUBDEV_QUERYCAP:
        print(put, *(v4l2_subdev_capability*)(argp));
        break;
    case VIDIOC_G_FMT:
    case VIDIOC_S_FMT:
    case VIDIOC_TRY_FMT:
        print(put, *(v4l2_format*)(argp));
        break;
    case VIDIOC_ENUM_FMT:
        print(put, *(v4l2_fmtdesc*)(argp));
        break;
    case VIDIOC_ENUM_FRAMESIZES:
        print(put, *(v4l2_frmsizeenum*)(argp));
        break;
    case VIDIOC_SUBDEV_ENUM_MBUS_CODE:
        print(put, *(v4l2_subdev_mbus_code_enum*)(argp));
        break;
    case VIDIOC_SUBDEV_ENUM_FRAME_SIZE:
        print(put, *(v4l2_subdev_frame_size_enum*)(argp));
        break;
    case VIDIOC_SUBDEV_G_FMT:
    case VIDIOC_SUBDEV_S_FMT:
        print(put, *(v4l2_subdev_format*)(argp));
        break;
    case VIDIOC_QUERYBUF:
        print(put, *(v4l2_buffer*)(argp));
        break;
    case VIDIOC_EXPBUF:
        print(put, *(v4l2_exportbuffer*)(argp));
        break;
    case VIDIOC_QBUF:
    case VIDIOC_DQBUF:
        print(put, *(v4l2_buffer*)(argp));
        break;
    }

    errno = e;
    return r;
}
} // namespace iocd

#pragma clang diagnostic pop
