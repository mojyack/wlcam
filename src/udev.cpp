#include <libudev.h>

#include "assert.hpp"
#include "udev.hpp"

namespace dev {
auto enumerate() -> std::unordered_map<dev_t, std::string> {
    auto ret = std::unordered_map<dev_t, std::string>();

    const auto lib = udev_new();
    DYN_ASSERT(lib != NULL);

    const auto udev_enum = udev_enumerate_new(lib);
    DYN_ASSERT(udev_enum != NULL);

    DYN_ASSERT(udev_enumerate_add_match_subsystem(udev_enum, "media") >= 0);
    DYN_ASSERT(udev_enumerate_add_match_subsystem(udev_enum, "video4linux") >= 0);
    DYN_ASSERT(udev_enumerate_add_match_is_initialized(udev_enum) >= 0);
    DYN_ASSERT(udev_enumerate_scan_devices(udev_enum) >= 0);

    const auto ents = udev_enumerate_get_list_entry(udev_enum);
    DYN_ASSERT(ents != NULL);

    for(auto ent = ents; ent; ent = udev_list_entry_get_next(ent)) {
        const auto syspath = udev_list_entry_get_name(ent);

        const auto dev = udev_device_new_from_syspath(lib, syspath);
        DYN_ASSERT(dev != NULL);

        const auto devnode = udev_device_get_devnode(dev);
        DYN_ASSERT(devnode != NULL);

        const auto devnum = udev_device_get_devnum(dev);

        ret[devnum] = devnode;

        udev_device_unref(dev);
    }

    udev_enumerate_unref(udev_enum);
    udev_unref(lib);

    return ret;
}
} // namespace dev
