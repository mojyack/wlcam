#pragma once
#include <thread>

#include "args.hpp"
#include "pulse-recorder/pulse.hpp"
#include "timer.hpp"
#include "video-encoder/converter.hpp"
#include "video-encoder/encoder.hpp"

struct RecordContext {
    ff::Encoder        encoder;
    ff::AudioConverter converter;
    pa::Recorder       recorder;
    Timer              timer;
    std::thread        recorder_thread;
    bool               running;

    auto recorder_main() -> bool;
    auto init(std::string path, AVPixelFormat pix_fmt, const CommonArgs& args) -> bool;

    ~RecordContext();
};
