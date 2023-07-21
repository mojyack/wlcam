#pragma once
#include "util/variant.hpp"

namespace RemoteEvents {
struct Hello {};

struct Configure {
    int fps;
};

struct RecordStart {
    std::string path;
};

struct RecordStop {
    std::string path;
};

struct Raw {
    std::string string;
};

struct Bye {};
}; // namespace RemoteEvents

using RemoteEvent = Variant<
    RemoteEvents::Hello,
    RemoteEvents::Configure,
    RemoteEvents::RecordStart,
    RemoteEvents::RecordStop,
    RemoteEvents::Raw,
    RemoteEvents::Bye>;

class RemoteServer {
  private:
    const char* path;
    int         fd = -1;

  public:
    operator bool() const {
        return fd != -1;
    }

    template <class T>
    auto send_event(T event) -> void {
        send_event(RemoteEvent(Tag<T>(), event));
    }

    auto send_event(RemoteEvent event) -> void;

    RemoteServer() = default;

    RemoteServer(const char* const path);

    ~RemoteServer();
};
