#pragma once
#include "../media-device.hpp"
#include "../util/fd.hpp"

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

struct Lens {
    std::string    dev_node;
    FileDescriptor fd;
};

struct Sensor {
    const Entity*       sensor;
    int                 pad_index;
    std::string         dev_node;
    FileDescriptor      fd;
    std::optional<Lens> lens;

    auto enum_pad_codes() const -> std::optional<std::vector<uint32_t>>;
    auto enum_pad_sizes(const uint32_t mbus_code) const -> std::optional<std::vector<SizeRange>>;

    static auto create(MediaDevice& media, const Entity& sensor) -> std::optional<Sensor>;
};

struct CIO2Device {
    MediaDevice*   media;
    FileDescriptor cio2;
    FileDescriptor output;
    Sensor         sensor;

    auto init(Entity* csi2) -> bool;
    auto init(std::string_view entity_name) -> bool;
    auto get_formats() const -> std::optional<std::vector<Format>>;

    CIO2Device(MediaDevice* const media)
        : media(media) {}
};
} // namespace ipu3::cio2
