#pragma once
#include <linux/videodev2.h>

enum class PixelFormat {
    MPEG,
    YUYV,
};

inline auto pixel_format_to_v4l2(const PixelFormat pixel_format) -> int {
    switch(pixel_format) {
    case PixelFormat::MPEG:
        return V4L2_PIX_FMT_MJPEG;
    case PixelFormat::YUYV:
        return V4L2_PIX_FMT_YUYV;
    }
}
