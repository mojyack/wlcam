#include <vector>

#include <libudev.h>
#include <linux/v4l2-subdev.h>

#include "../ioutil.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"
#include "../util/print.hpp"
#include "cio2.hpp"
#include "imgu.hpp"

namespace {
auto read_sized(const std::string_view prompt, const size_t limit) -> size_t {
    while(true) {
        print(prompt, ": [0..", limit - 1, "] ");
        const auto i = stdio::read_stdin<size_t>();
        if(i < limit) {
            return i;
        }
        print("invalid input");
    }
}
} // namespace

auto main() -> int {
    print("searching for ipu3 devices");

    const auto udev_devices = dev::enumerate();
    auto       csi2_devices = std::vector<MediaDevice>();
    auto       imgu_devices = std::vector<MediaDevice>();
    for(const auto& [devnum, devnode] : udev_devices) {
        if(major(devnum) != 246) {
            continue;
        }

        auto media_deivce = parse_device(devnode.c_str(), udev_devices);
        if(media_deivce.find_entity_by_name("ipu3-csi2 0") != nullptr) {
            print("found csi2");
            csi2_devices.emplace_back(media_deivce);
        } else if(media_deivce.find_entity_by_name("ipu3-imgu 0") != nullptr) {
            print("found imgu");
            imgu_devices.emplace_back(media_deivce);
        }
    }

    if(csi2_devices.empty()) {
        print("no csi2 device found");
        return 1;
    }
    if(imgu_devices.empty()) {
        print("no imgu device found");
        return 1;
    }

    auto& csi2_device = csi2_devices[read_sized("select csi2 device", csi2_devices.size())];
    auto& imgu_device = imgu_devices[read_sized("select imgu device", imgu_devices.size())];

    csi2_device.disable_all_links();
    imgu_device.disable_all_links();

    auto sensors = std::vector<const Entity*>();
    for(auto i = 0; i < 4; i += 1) {
        const auto csi2 = csi2_device.find_entity_by_name(build_string("ipu3-csi2 ", i));
        if(csi2 == nullptr) {
            continue;
        }

        if(csi2->pads.empty() || csi2->pads[0].links.empty()) {
            continue;
        }

        const auto& link        = csi2->pads[0].links[0];
        const auto [sensor, _1] = csi2_device.find_pad_owner_and_index(link.src_pad_id);
        if(sensor == nullptr) {
            continue;
        }
        sensors.emplace_back(sensor);
    }

    print("the csi2 has ", sensors.size(), " sensors");
    for(auto i = 0u; i < sensors.size(); i += 1) {
        print("  ", i, " ", sensors[i]->name);
    }
    const auto sensor = sensors[read_sized("select sensor", sensors.size())];

    const auto [csi2_entity, _1] = csi2_device.find_pad_owner_and_index(sensor->pads[0].links[0].sink_pad_id);

    constexpr auto imgu_entitiy_name = "ipu3-imgu 0";
    auto           imgu              = ipu3::ImgUDevice();
    imgu.init(imgu_device, imgu_entitiy_name); // only use 0th imgu

    auto cio2 = ipu3::cio2::CIO2Device(&csi2_device);
    cio2.init(const_cast<Entity*>(csi2_entity));

    const auto sensor_formats = cio2.get_formats();
    print("the sensor suppors ", sensor_formats.size(), " formats");
    for(auto i = 0u; i < sensor_formats.size(); i += 1) {
        print("  ", i, " ", sensor_formats[i].code);
    }
    const auto& sensor_format = sensor_formats[read_sized("select media bus code", sensor_formats.size())];

    print("the sensor suppors ", sensor_format.sizes.size(), " output sizes");
    for(auto i = 0u; i < sensor_format.sizes.size(); i += 1) {
        print("  ", i, " ", sensor_format.sizes[i].max.width, "x", sensor_format.sizes[i].max.height);
    }
    const auto& sensor_format_size = sensor_format.sizes[read_sized("select output size", sensor_format.sizes.size())];

    const auto width  = stdio::read_stdin<size_t>("imgu output width: ");
    const auto height = stdio::read_stdin<size_t>("imgu output height: ");

    print(
        "launch wlcam-ipu3 with these arguments:\n",
        " --cio2 ", csi2_device.dev_node,
        " --imgu ", imgu_device.dev_node,
        " --cio2-entity \"", csi2_entity->name, "\"",
        " --imgu-entity \"", imgu_entitiy_name, "\"",
        " --sensor-mbus-code ", sensor_format.code,
        " --sensor-width ", sensor_format_size.max.width,
        " --sensor-height ", sensor_format_size.max.height,
        " --width ", width,
        " --height ", height);

    print("controls available for these devices:");
    print("  ", cio2.sensor.dev_node);
    if(cio2.sensor.lens) {
        print("  ", cio2.sensor.lens->dev_node);
    }

    return 0;
}
