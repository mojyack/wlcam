#pragma once
#include <atomic>
#include <thread>

#include <GL/gl.h>

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
    std::atomic<bool>  audio_started = false;
    bool               running;

    // private
    auto recorder_main() -> bool;
    auto init(std::string path, ff::VideoParams vopts, const CommonArgs& args) -> bool;

    // internal video encoder
    auto init(std::string path, AVPixelFormat pix_fmt, int width, int height, const CommonArgs& args) -> bool;
    // external video encoder
    auto init(std::string path, int real_width, int real_height, int coded_width, int coded_height, const CommonArgs& args) -> bool;
    // start recording if ready
    auto ensure_recording() -> void;

    ~RecordContext();
};
