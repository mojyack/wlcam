#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../ioutil.hpp"
#include "../macros/unwrap.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"

namespace {
auto read_sized(const std::string_view prompt, const size_t limit) -> size_t {
    if(limit == 1) {
        return 0;
    }
    while(true) {
        const auto i = stdio::read_stdin<size_t>(std::format("{} [0..{}]: ", prompt, limit - 1));
        if(i < limit) {
            return i;
        }
        std::println("invalid input");
    }
}

auto source_pad_index(const Entity& entity) -> int {
    for(auto i = 0u; i < entity.pads.size(); i += 1) {
        if(entity.pads[i].flags & MEDIA_PAD_FL_SOURCE) {
            return i;
        }
    }
    return -1;
}

// The sensor feeding a csiphy's sink pad.
auto find_sensor(MediaDevice& media, Entity& csiphy) -> const Entity* {
    for(auto& pad : csiphy.pads) {
        if(!(pad.flags & MEDIA_PAD_FL_SINK)) {
            continue;
        }
        for(auto& link : pad.links) {
            if(link.sink_pad_id != pad.id) {
                continue;
            }
            const auto [src, _] = media.find_pad_owner_and_index(link.src_pad_id);
            return src;
        }
    }
    return nullptr;
}
} // namespace

auto main() -> int {
    unwrap(node_map, dev::enumerate());

    // locate the qcom-camss media device
    auto camss = std::optional<MediaDevice>();
    for(const auto& [devnum, devnode] : node_map) {
        if(!devnode.starts_with("/dev/media")) {
            continue;
        }
        auto md = parse_device(devnode.c_str(), node_map);
        if(md && md->driver == "qcom-camss") {
            camss.emplace(std::move(*md));
            break;
        }
    }
    ensure(camss, "no qcom-camss media device found");
    auto& media = *camss;
    // media.debug_print();

    // one sensor per csiphy
    struct CSIPhyPair {
        const Entity* csiphy; // sink
        const Entity* sensor; // src
    };
    auto pairs = std::vector<CSIPhyPair>();
    for(auto& entity : media.entities) {
        if(!entity.name.starts_with("msm_csiphy")) {
            continue;
        }
        const auto sensor = find_sensor(media, entity);
        if(sensor != nullptr) {
            pairs.emplace_back(CSIPhyPair{&entity, sensor});
        }
    }
    ensure(!pairs.empty(), "no sensors found");

    std::println("sensors:");
    for(auto i = 0u; i < pairs.size(); i += 1) {
        std::println("  {}  {} -> {}", i, pairs[i].sensor->name, pairs[i].csiphy->name);
    }
    const auto [csiphy, sensor] = pairs[read_sized("select sensor", pairs.size())];

    const auto fd = open(sensor->dev_node.c_str(), O_RDWR);
    ensure(fd >= 0, "failed to open {}", sensor->dev_node);
    const auto pad = source_pad_index(*sensor);
    ensure(pad >= 0, "sensor has no source pad");

    // media-bus codes
    auto codes = std::vector<uint32_t>();
    for(auto i = 0u;; i += 1) {
        auto e  = v4l2_subdev_mbus_code_enum{};
        e.pad   = pad;
        e.index = i;
        e.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        if(ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &e) != 0) {
            break;
        }
        codes.push_back(e.code);
    }
    ensure(!codes.empty(), "sensor exposes no media-bus codes");
    std::println("media-bus codes:");
    for(auto i = 0u; i < codes.size(); i += 1) {
        std::println("  {}  0x{:04x}", i, codes[i]);
    }
    const auto code = codes[read_sized("select media-bus code", codes.size())];

    // frame sizes for the chosen code
    auto sizes = std::vector<std::pair<uint32_t, uint32_t>>();
    for(auto i = 0u;; i += 1) {
        auto e  = v4l2_subdev_frame_size_enum{};
        e.pad   = pad;
        e.index = i;
        e.code  = code;
        e.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        if(ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &e) != 0) {
            break;
        }
        sizes.emplace_back(e.max_width, e.max_height);
    }
    ensure(!sizes.empty(), "sensor exposes no frame sizes");
    std::println("frame sizes:");
    for(auto i = 0u; i < sizes.size(); i += 1) {
        std::println("  {}  {}x{}", i, sizes[i].first, sizes[i].second);
    }
    const auto size = sizes[read_sized("select frame size", sizes.size())];

    close(fd);

    std::println("");
    std::println("launch with:");
    std::println("  wlcam-camss --csiphy {} --width {} --height {}", csiphy->name, size.first, size.second);
    std::println("control device nodes:");
    std::println("  sensor: {}", sensor->dev_node);
    if(!sensor->ancillary_entities.empty()) {
        const auto lens = media.find_entity_by_id(sensor->ancillary_entities[0]);
        if(lens != nullptr) {
            std::println("  lens:   {}", lens->dev_node);
        }
    }

    return 0;
}
