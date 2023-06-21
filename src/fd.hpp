#pragma once
#include <utility>

#include <unistd.h>

struct FileDescriptor {
    int fd = -1;

    auto close() -> void {
        if(fd != -1) {
            ::close(fd);
        }
    }

    operator int() const {
        return fd;
    }

    auto operator=(FileDescriptor&& other) {
        close();
        fd = std::exchange(other.fd, -1);
    }

    FileDescriptor() = default;

    explicit FileDescriptor(const int fd)
        : fd(fd) {}

    FileDescriptor(FileDescriptor&& other)
        : fd(std::exchange(other.fd, -1)) {}

    ~FileDescriptor() {
        close();
    }
};
