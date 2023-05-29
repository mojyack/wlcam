#pragma once
#include <gawl/pixelbuffer.hpp>

#include "pixel-format.hpp"
#include "util/thread.hpp"

enum class Command {
    None,
    TakePhoto,
    TakePhotoDone,
    StartRecording,
    StartRecordingDone,
    StopRecording,
    StopRecordingDone,
};

struct PixelBuffers {
    gawl::PixelBuffer y;
    gawl::PixelBuffer u;
    gawl::PixelBuffer v;
};

struct Context {
    Critical<PixelBuffers> critical_pixel_buffers;
    PixelFormat            pixel_format;
    Command                ui_command;     // camera -> ui
    Command                camera_command; // ui -> camera
};
