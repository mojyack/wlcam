#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>

#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "cio2.hpp"

namespace ipu3::cio2 {
auto Sensor::enum_pad_codes() const -> std::optional<std::vector<uint32_t>> {
    auto ret = std::vector<uint32_t>();

    for(auto i = 0u;; i += 1) {
        auto mbus_enum  = v4l2_subdev_mbus_code_enum();
        mbus_enum.pad   = pad_index;
        mbus_enum.index = i;
        mbus_enum.which = V4L2_SUBDEV_FORMAT_ACTIVE;

        if(ioctl(fd.as_handle(), VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_enum) < 0) {
            assert_o(errno == EINVAL);
            break;
        }

        ret.emplace_back(mbus_enum.code);
    }

    return ret;
}

auto Sensor::enum_pad_sizes(const uint32_t mbus_code) const -> std::optional<std::vector<SizeRange>> {
    auto ret = std::vector<SizeRange>();

    for(auto i = 0u;; i += 1) {
        auto size_enum  = v4l2_subdev_frame_size_enum();
        size_enum.index = i;
        size_enum.pad   = pad_index;
        size_enum.code  = mbus_code;
        size_enum.which = V4L2_SUBDEV_FORMAT_ACTIVE;

        if(ioctl(fd.as_handle(), VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &size_enum) < 0) {
            assert_o(errno == EINVAL || errno == ENOTTY);
            break;
        }

        ret.emplace_back(SizeRange{{size_enum.min_width, size_enum.min_height}, {size_enum.max_width, size_enum.max_height}});
    }

    return ret;
}

auto Sensor::create(MediaDevice& media, const Entity& sensor) -> std::optional<Sensor> {
    assert_o(sensor.function == MEDIA_ENT_F_CAM_SENSOR || sensor.function == MEDIA_ENT_F_PROC_VIDEO_ISP);
    auto       fdh = FileDescriptor(open(sensor.dev_node.data(), O_RDWR));
    const auto fd  = fdh.as_handle();
    assert_o(fd >= 0);

    auto pad_index = -1;
    for(auto p = 0u; p < sensor.pads.size(); p += 1) {
        if(sensor.pads[p].flags & MEDIA_PAD_FL_SOURCE) {
            pad_index = p;
            break;
        }
    }
    assert_o(pad_index != -1);

    auto lens = std::optional<Lens>();
    if(!sensor.ancillary_entities.empty()) {
        const auto& dev_node = media.find_entity_by_id(sensor.ancillary_entities[0])->dev_node;
        const auto  fd       = open(media.find_entity_by_id(sensor.ancillary_entities[0])->dev_node.c_str(), O_RDWR);
        lens.emplace(Lens{dev_node, FileDescriptor{fd}});
    }

    return Sensor{&sensor, pad_index, sensor.dev_node, std::move(fdh), std::move(lens)};
}

auto CIO2Device::init(Entity* const csi2) -> bool {
    auto& link = csi2->pads[0].links[0]; // link to sensor

    const auto [sensor_ent, _1] = media->find_pad_owner_and_index(link.src_pad_id);
    assert_b(sensor_ent != nullptr);

    assert_b(media->configure_link(link, true));

    cio2 = FileDescriptor(open(csi2->dev_node.data(), O_RDWR));
    assert_b(cio2.as_handle() >= 0);

    unwrap_ob_mut(sensor_obj, Sensor::create(*media, *sensor_ent));
    sensor = std::move(sensor_obj);

    const auto output_ent = media->find_entity_by_name(build_string("ipu3-cio2 ", csi2->name.back()));
    output                = FileDescriptor(open(output_ent->dev_node.data(), O_RDWR));
    assert_b(output.as_handle() >= 0);

    return true;
}

auto CIO2Device::init(const std::string_view entity_name) -> bool {
    const auto csi2 = media->find_entity_by_name(build_string(entity_name));
    assert_b(csi2 != nullptr);

    return init(csi2);
}

auto CIO2Device::get_formats() const -> std::optional<std::vector<Format>> {
    auto ret = std::vector<Format>();
    unwrap_oo(pad_codes, sensor.enum_pad_codes());

    for(const auto c : pad_codes) {
        auto& f = ret.emplace_back(Format{c, {}});
        unwrap_oo(pad_sizes, sensor.enum_pad_sizes(c));
        for(const auto r : pad_sizes) {
            f.sizes.emplace_back(r);
        }
    }

    return ret;
}
} // namespace ipu3::cio2
