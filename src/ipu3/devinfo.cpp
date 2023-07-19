#include <vector>

#include <libudev.h>
#include <linux/v4l2-subdev.h>

#include "../ioutil.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"
#include "../util/error.hpp"
#include "../v4l2.hpp"
#include "cio2.hpp"
#include "imgu.hpp"

auto print_sensor_devnode(const uint32_t cio2_index, const uint32_t sensor_index) -> void {
    const auto udev_devices = dev::enumerate();
    auto       csi2_devices = std::vector<MediaDevice>();
    for(const auto& [devnum, devnode] : udev_devices) {
        if(major(devnum) != 246) {
            continue;
        }

        auto media_deivce = parse_device(devnode.c_str(), udev_devices);
        if(media_deivce.find_entity_by_name("ipu3-csi2 0") != nullptr) {
            csi2_devices.emplace_back(media_deivce);
        }
    }

    if(cio2_index >= csi2_devices.size()) {
        return;
    }

    auto& csi2_device = csi2_devices[cio2_index];

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

    if(sensor_index >= sensors.size()) {
        return;
    }

    const auto sensor = sensors[sensor_index];

    const auto [csi2_entity, _1] = csi2_device.find_pad_owner_and_index(sensor->pads[0].links[0].sink_pad_id);

    auto cio2 = ipu3::cio2::CIO2Device(&csi2_device);
    cio2.init(const_cast<Entity*>(csi2_entity));

    stdio::print(cio2.sensor.dev_node);
    if(cio2.sensor.lens) {
        stdio::print("  ", cio2.sensor.lens->dev_node);
    }
    stdio::println();
}

auto main(const int argc, const char* const argv[]) -> int {
    if(argc <= 1) {
        return 1;
    }

    if(std::strcmp(argv[1], "sensor-devnode") == 0) {
        if(argc <= 3) {
            return 1;
        }
        const auto i = stdio::from_chars<int>(argv[2]);
        const auto j = stdio::from_chars<int>(argv[3]);
        if(i && j) {
            print_sensor_devnode(*i, *j);
        }
        return 1;
    }
    return 0;
}
