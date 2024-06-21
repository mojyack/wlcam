#pragma once
#include "graphics-wrapper.hpp"

enum class Command {
    None,
    TakePhoto,
    TakePhotoDone,
    StartRecording,
    StartRecordingDone,
    StopRecording,
    StopRecordingDone,
};

struct Context {
    std::shared_ptr<Frame> frame;
    Command                ui_command;     // camera -> ui
    Command                camera_command; // ui -> camera
};
