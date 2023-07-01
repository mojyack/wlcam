#pragma once
#include "graphics/planar.hpp"
#include "graphics/yuv420sp.hpp"
#include "graphics/yuv422i.hpp"
#include "util/variant.hpp"

enum class Command {
    None,
    TakePhoto,
    TakePhotoDone,
    StartRecording,
    StartRecordingDone,
    StopRecording,
    StopRecordingDone,
};

using GraphicLike = Variant<PlanarGraphic, YUV420spGraphic, YUV422iGraphic>;

struct Context {
    std::shared_ptr<GraphicLike> critical_graphic;
    Command                      ui_command;     // camera -> ui
    Command                      camera_command; // ui -> camera
};
