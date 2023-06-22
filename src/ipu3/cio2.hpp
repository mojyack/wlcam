#pragma once
#include "../fd.hpp"
#include "../media-device.hpp"

namespace ipu3::cio2 {
struct Size {
    uint32_t width;
    uint32_t height;
};

struct SizeRange {
    Size min;
    Size max;
};

struct Format {
    uint32_t               code;
    std::vector<SizeRange> sizes;
};

struct Sensor {
    const Entity*  sensor;
    FileDescriptor fd;
    int            pad_index;

    auto enum_pad_codes() const -> std::vector<uint32_t>;

    auto enum_pad_sizes(const uint32_t mbus_code) const -> std::vector<SizeRange>;

    static auto create(const Entity& sensor) -> Sensor;
};

struct CIO2Device {
    MediaDevice*   media;
    FileDescriptor cio2;
    FileDescriptor output;
    Sensor         sensor;

    auto init(Entity* csi2) -> void;

    auto init(std::string_view entity_name) -> void;

    auto get_formats() const -> std::vector<Format>;

    CIO2Device(MediaDevice* const media)
        : media(media) {}
};
} // namespace cio2
