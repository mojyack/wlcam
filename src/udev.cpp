#include <libudev.h>

#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"
#include "udev.hpp"
#include "util/assert.hpp"

declare_autoptr(Udev, udev, udev_unref);
declare_autoptr(UdevEnum, udev_enumerate, udev_enumerate_unref);
declare_autoptr(UdevDevice, udev_device, udev_device_unref);

namespace dev {
auto enumerate() -> std::optional<std::unordered_map<dev_t, std::string>> {
    auto ret = std::unordered_map<dev_t, std::string>();

    auto lib = AutoUdev(udev_new());
    assert_o(lib.get() != NULL);

    auto udev_enum = AutoUdevEnum(udev_enumerate_new(lib.get()));
    assert_o(udev_enum.get() != NULL);

    assert_o(udev_enumerate_add_match_subsystem(udev_enum.get(), "media") >= 0);
    assert_o(udev_enumerate_add_match_subsystem(udev_enum.get(), "video4linux") >= 0);
    assert_o(udev_enumerate_add_match_is_initialized(udev_enum.get()) >= 0);
    assert_o(udev_enumerate_scan_devices(udev_enum.get()) >= 0);

    unwrap_po_mut(ents, udev_enumerate_get_list_entry(udev_enum.get()));

    for(auto ent = &ents; ent; ent = udev_list_entry_get_next(ent)) {
        const auto syspath = udev_list_entry_get_name(ent);

        auto dev = AutoUdevDevice(udev_device_new_from_syspath(lib.get(), syspath));
        assert_o(dev.get() != NULL);

        unwrap_po_mut(devnode, udev_device_get_devnode(dev.get()));
        const auto devnum = udev_device_get_devnum(dev.get());
        ret[devnum]       = devnode;
    }

    return ret;
}
} // namespace dev
