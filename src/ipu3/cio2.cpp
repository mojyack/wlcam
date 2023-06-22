#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>

#include "../assert.hpp"
#include "cio2.hpp"

namespace ipu3::cio2 {
auto Sensor::enum_pad_codes() const -> std::vector<uint32_t> {
    auto ret = std::vector<uint32_t>();

    for(auto i = 0u;; i += 1) {
        auto mbus_enum  = v4l2_subdev_mbus_code_enum();
        mbus_enum.pad   = pad_index;
        mbus_enum.index = i;
        mbus_enum.which = V4L2_SUBDEV_FORMAT_ACTIVE;

        if(ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_enum) < 0) {
            DYN_ASSERT(errno == EINVAL);
            break;
        }

        ret.emplace_back(mbus_enum.code);
    }

    return ret;
}

auto Sensor::enum_pad_sizes(const uint32_t mbus_code) const -> std::vector<SizeRange> {
    auto ret = std::vector<SizeRange>();

    for(auto i = 0u;; i += 1) {
        auto size_enum  = v4l2_subdev_frame_size_enum();
        size_enum.index = i;
        size_enum.pad   = pad_index;
        size_enum.code  = mbus_code;
        size_enum.which = V4L2_SUBDEV_FORMAT_ACTIVE;

        if(ioctl(fd, VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &size_enum) < 0) {
            DYN_ASSERT(errno == EINVAL || errno == ENOTTY);
            break;
        }

        ret.emplace_back(SizeRange{{size_enum.min_width, size_enum.min_height}, {size_enum.max_width, size_enum.max_height}});
    }

    return ret;
}

auto Sensor::create(const Entity& sensor) -> Sensor {
    DYN_ASSERT(sensor.function == MEDIA_ENT_F_CAM_SENSOR || sensor.function == MEDIA_ENT_F_PROC_VIDEO_ISP);
    const int fd = open(sensor.dev_node.data(), O_RDWR);
    DYN_ASSERT(fd >= 0);

    auto pad_index = -1;
    for(auto p = 0u; p < sensor.pads.size(); p += 1) {
        if(sensor.pads[p].flags & MEDIA_PAD_FL_SOURCE) {
            pad_index = p;
            break;
        }
    }
    DYN_ASSERT(pad_index != -1);

    return Sensor{&sensor, FileDescriptor(fd), pad_index};
}

auto CIO2Device::init(Entity* const csi2) -> void {
    auto& link = csi2->pads[0].links[0]; // link to sensor

    const auto [sensor, _1] = media->find_pad_owner_and_index(link.src_pad_id);
    DYN_ASSERT(sensor != nullptr);

    media->configure_link(link, true);

    cio2 = FileDescriptor(open(csi2->dev_node.data(), O_RDWR));
    DYN_ASSERT(cio2 >= 0);

    this->sensor = Sensor::create(*sensor);

    const auto output = media->find_entity_by_name(build_string("ipu3-cio2 ", csi2->name.back()));
    this->output      = FileDescriptor(open(output->dev_node.data(), O_RDWR));
    DYN_ASSERT(this->output >= 0);
}

auto CIO2Device::init(const std::string_view entity_name) -> void {
    const auto csi2 = media->find_entity_by_name(build_string(entity_name));
    DYN_ASSERT(csi2 != nullptr);

    return init(csi2);
}

auto CIO2Device::get_formats() const -> std::vector<Format> {
    auto ret = std::vector<Format>();

    for(const auto c : sensor.enum_pad_codes()) {
        auto& f = ret.emplace_back(Format{c, {}});
        for(const auto r : sensor.enum_pad_sizes(c)) {
            f.sizes.emplace_back(r);
        }
    }

    return ret;
}
} // namespace cio2
