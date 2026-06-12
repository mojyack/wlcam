#pragma once
#include <memory>

extern "C" {
#include <libavutil/frame.h>
}

namespace ff {
#define av_declare_autoptr(Name, Type, func) \
    struct Name##Deleter {                   \
        auto operator()(Type* ptr) -> void { \
            func(&ptr);                      \
        }                                    \
    };                                       \
    using Auto##Name = std::unique_ptr<Type, Name##Deleter>;

av_declare_autoptr(AVFrame, AVFrame, av_frame_free);
}
