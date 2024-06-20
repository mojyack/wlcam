#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "macros/assert.hpp"
#include "media-device.hpp"
#include "util/assert.hpp"

namespace {
auto find_interface(const media_v2_topology& topology, const uint32_t entity_id) -> Interface {
    for(auto l = 0u; l < topology.num_links; l += 1) {
        const auto& lp = (std::bit_cast<media_v2_link*>(topology.ptr_links))[l];
        if((lp.flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_INTERFACE_LINK) {
            continue;
        }
        if(lp.sink_id != entity_id) {
            continue;
        }
        for(auto i = 0u; i < topology.num_interfaces; i += 1) {
            const auto& ip = (std::bit_cast<media_v2_interface*>(topology.ptr_interfaces))[i];
            if(ip.id == lp.source_id) {
                return {ip.devnode.major, ip.devnode.minor, ip.intf_type};
            }
        }
        PANIC("unknown interface");
    }
    PANIC("the entity has no interface");
}

auto parse_topology(const media_v2_topology& topology, const std::unordered_map<dev_t, std::string>& node_map) -> std::vector<Entity> {
    auto entities = std::vector<Entity>(topology.num_entities);
    for(auto e = 0u; e < topology.num_entities; e += 1) {
        const auto& ep        = (std::bit_cast<media_v2_entity*>(topology.ptr_entities))[e];
        const auto  interface = find_interface(topology, ep.id);
        const auto  dev_node  = node_map.find(makedev(interface.major, interface.minor))->second;

        auto& entity = entities[e];
        entity       = Entity{ep.id, ep.function, interface, ep.name, {}, dev_node, {}};

        for(auto p = 0u; p < topology.num_pads; p += 1) {
            const auto& pp = (std::bit_cast<media_v2_pad*>(topology.ptr_pads))[p];
            if(pp.entity_id != ep.id) {
                continue;
            }
            auto& pad = entities[e].pads.emplace_back(Pad{pp.id, pp.flags, {}});

            for(auto l = 0u; l < topology.num_links; l += 1) {
                const auto& lp = (std::bit_cast<media_v2_link*>(topology.ptr_links))[l];
                if((lp.flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_DATA_LINK) {
                    continue;
                }
                if(lp.source_id != pp.id && lp.sink_id != pp.id) {
                    continue;
                }
                pad.links.emplace_back(Link{lp.id, lp.source_id, lp.sink_id, lp.flags});
            }
        }

        for(auto l = 0u; l < topology.num_links; l += 1) {
            const auto& lp = (std::bit_cast<media_v2_link*>(topology.ptr_links))[l];
            if((lp.flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_ANCILLARY_LINK) {
                continue;
            }
            if(lp.source_id != entity.id) {
                continue;
            }
            entity.ancillary_entities.emplace_back(lp.sink_id);
        }
    }

    return entities;
}
} // namespace

auto MediaDevice::find_entity_by_id(const uint32_t id) -> Entity* {
    auto i = std::find_if(entities.begin(), entities.end(), [id](const Entity& e) { return e.id == id; });
    return i == entities.end() ? nullptr : &*i;
}

auto MediaDevice::find_entity_by_name(std::string_view name) -> Entity* {
    auto i = std::find_if(entities.begin(), entities.end(), [&name](const Entity& e) { return e.name == name; });
    return i == entities.end() ? nullptr : &*i;
}

auto MediaDevice::find_pad_owner_and_index(const uint32_t pad_id) const -> std::tuple<const Entity*, size_t> {
    for(const auto& e : entities) {
        for(auto ip = 0u; ip < e.pads.size(); ip += 1) {
            auto& p = e.pads[ip];
            if(p.id == pad_id) {
                return {&e, ip};
            }
        }
    }

    return {nullptr, 0};
}

auto MediaDevice::configure_link(Link& link, const bool enable) -> void {
    auto desc               = media_link_desc();
    auto [src, src_index]   = find_pad_owner_and_index(link.src_pad_id);
    auto [sink, sink_index] = find_pad_owner_and_index(link.sink_pad_id);
    desc.source             = {src->id, uint16_t(src_index), MEDIA_PAD_FL_SOURCE, {}};
    desc.sink               = {sink->id, uint16_t(sink_index), MEDIA_PAD_FL_SINK, {}};
    desc.flags              = (link.flags & ~MEDIA_LNK_FL_ENABLED) | (enable ? MEDIA_LNK_FL_ENABLED : 0);
    DYN_ASSERT(ioctl(fd, MEDIA_IOC_SETUP_LINK, &desc) == 0);

    link.flags = desc.flags;
}

auto MediaDevice::disable_all_links() -> void {
    for(auto& e : entities) {
        for(auto ip = 0u; ip < e.pads.size(); ip += 1) {
            auto& p = e.pads[ip];
            if(!(p.flags & MEDIA_PAD_FL_SOURCE)) {
                continue;
            }
            for(auto& l : p.links) {
                if(l.flags & MEDIA_LNK_FL_IMMUTABLE) {
                    continue;
                }
                configure_link(l, false);
            }
        }
    }
}

auto MediaDevice::debug_print() const -> void {
    print("driver: ", driver);
    for(auto& e : entities) {
        print("  entity ", e.id, ": ", e.name, " ", e.dev_node);
        // continue;
        for(auto& p : e.pads) {
            print("    pad ", p.id);
            for(auto& l : p.links) {
                if(l.src_pad_id == p.id) {
                    const auto& other_name = std::get<0>(find_pad_owner_and_index(l.sink_pad_id))->name;
                    print("      link ", l.id, ": -> ", l.sink_pad_id, "(", other_name, ")");
                } else {
                    const auto& other_name = std::get<0>(find_pad_owner_and_index(l.src_pad_id))->name;
                    print("      link ", l.id, ": <- ", l.src_pad_id, "(", other_name, ")");
                }
            }
        }

        if(!e.ancillary_entities.empty()) {
            print("    ancillary");
            for(const auto anc : e.ancillary_entities) {
                const auto other = const_cast<MediaDevice*>(this)->find_entity_by_id(anc);
                print("      ", other->id, "(", other->name, ")");
            }
        }
    }
}

auto parse_device(const char* const device, const std::unordered_map<dev_t, std::string>& node_map) -> MediaDevice {
    const int fd = open(device, O_RDWR);
    DYN_ASSERT(fd >= 0);

    auto info = media_device_info();
    DYN_ASSERT(ioctl(fd, MEDIA_IOC_DEVICE_INFO, &info) == 0);

    auto topology = media_v2_topology();
    DYN_ASSERT(ioctl(fd, MEDIA_IOC_G_TOPOLOGY, &topology) == 0);

    auto entities   = std::vector<media_v2_entity>(topology.num_entities);
    auto interfaces = std::vector<media_v2_interface>(topology.num_interfaces);
    auto links      = std::vector<media_v2_link>(topology.num_links);
    auto pads       = std::vector<media_v2_pad>(topology.num_pads);

    topology.ptr_entities   = std::bit_cast<uintptr_t>(entities.data());
    topology.ptr_interfaces = std::bit_cast<uintptr_t>(interfaces.data());
    topology.ptr_links      = std::bit_cast<uintptr_t>(links.data());
    topology.ptr_pads       = std::bit_cast<uintptr_t>(pads.data());
    DYN_ASSERT(ioctl(fd, MEDIA_IOC_G_TOPOLOGY, &topology) == 0);

    return {device, info.driver, parse_topology(topology, node_map), fd};
}
