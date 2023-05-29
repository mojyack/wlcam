#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "assert.hpp"
#include "remote-server.hpp"

namespace {
template <size_t n>
constexpr auto write_string(const int fd, const char (&a)[n]) -> void {
    DYN_ASSERT(write(fd, a, n - 1) == n - 1);
}

auto write_string(const int fd, const char* const ptr, const size_t size) -> void {
    DYN_ASSERT(write(fd, ptr, size) == ssize_t(size));
}
} // namespace

auto RemoteServer::send_event(const RemoteEvent event) -> void {
    switch(event.get_index()) {
    case RemoteEvent::index_of<RemoteEvents::Hello>:
        write_string(fd, "hello\n");
        break;
    case RemoteEvent::index_of<RemoteEvents::Configure>: {
        const auto& args = event.as<RemoteEvents::Configure>();
        const auto  str  = build_string("configure fps:", args.fps, "\n");
        write_string(fd, str.data(), str.size());
        break;
    }
    case RemoteEvent::index_of<RemoteEvents::RecordStart>: {
        const auto& path = event.as<RemoteEvents::RecordStop>().path;
        write_string(fd, "record_start ");
        write_string(fd, path.data(), path.size());
        write_string(fd, "\n");
    } break;
    case RemoteEvent::index_of<RemoteEvents::RecordStop>: {
        const auto& path = event.as<RemoteEvents::RecordStop>().path;
        write_string(fd, "record_stop ");
        write_string(fd, path.data(), path.size());
        write_string(fd, "\n");
    } break;
    case RemoteEvent::index_of<RemoteEvents::Bye>:
        write_string(fd, "bye\n");
        break;
    }
}

RemoteServer::RemoteServer(const char* const path)
    : path(path) {
    DYN_ASSERT(mkfifo(path, 0644) == 0, "cannot create fifo, maybe it exits?");
    print("waiting for remote client");
    fd = open(path, O_WRONLY);
    DYN_ASSERT(fd > 0);
}

RemoteServer::~RemoteServer() {
    if(fd != -1) {
        close(fd);
        DYN_ASSERT(unlink(path) == 0);
    }
}
