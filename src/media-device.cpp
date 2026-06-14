#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "macros/unwrap.hpp"
#include "media-device.hpp"

namespace {
auto find_interface(const media_v2_topology& topology, const uint32_t entity_id) -> std::optional<Interface> {
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
                return Interface{ip.devnode.major, ip.devnode.minor, ip.intf_type};
            }
        }
        bail("unknown interface");
    }
    bail("the entity has no interface");
}

auto parse_topology(const media_v2_topology& topology, const std::unordered_map<dev_t, std::string>& node_map) -> std::optional<std::vector<Entity>> {
    auto entities = std::vector<Entity>(topology.num_entities);
    for(auto e = 0u; e < topology.num_entities; e += 1) {
        const auto& ep = (std::bit_cast<media_v2_entity*>(topology.ptr_entities))[e];
        unwrap(interface, find_interface(topology, ep.id));
        const auto dev_node = node_map.find(makedev(interface.major, interface.minor))->second;

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

auto MediaDevice::find_links(const Entity* const src, const Entity* const sink) const -> std::vector<LinkInfo> {
    auto ret = std::vector<LinkInfo>();
    if(src != nullptr) {
        for(auto p = uint32_t(0); p < src->pads.size(); p += 1) {
            const auto& pad = src->pads[p];
            if(!(pad.flags & MEDIA_PAD_FL_SOURCE)) {
                continue;
            }
            for(auto l = uint32_t(0); l < pad.links.size(); l += 1) {
                const auto& link         = pad.links[l];
                const auto [peer, index] = find_pad_owner_and_index(link.sink_pad_id);
                if(sink == nullptr || peer == sink) {
                    ret.emplace_back(LinkInfo{
                        .src_entity   = src,
                        .sink_entity  = peer,
                        .src_pad_num  = p,
                        .sink_pad_num = uint32_t(index),
                        .link         = &link,
                    });
                }
            }
        }
    } else if(sink != nullptr) {
        for(auto p = uint32_t(0); p < sink->pads.size(); p += 1) {
            const auto& pad = sink->pads[p];
            if(!(pad.flags & MEDIA_PAD_FL_SINK)) {
                continue;
            }
            for(auto l = uint32_t(0); l < pad.links.size(); l += 1) {
                const auto& link         = pad.links[l];
                const auto [peer, index] = find_pad_owner_and_index(link.src_pad_id);
                if(src == nullptr || peer == src) {
                    ret.emplace_back(LinkInfo{
                        .src_entity   = peer,
                        .sink_entity  = sink,
                        .src_pad_num  = uint32_t(index),
                        .sink_pad_num = p,
                        .link         = &link,
                    });
                }
            }
        }
    }
    return ret;
}

auto MediaDevice::find_link(const Entity* const src, const Entity* const sink) const -> std::optional<LinkInfo> {
    auto links = find_links(src, sink);
    ensure(links.size() == 1);
    return links[0];
}

auto MediaDevice::configure_link(const Link& link, const bool enable) -> bool {
    auto desc               = media_link_desc();
    auto [src, src_index]   = find_pad_owner_and_index(link.src_pad_id);
    auto [sink, sink_index] = find_pad_owner_and_index(link.sink_pad_id);
    desc.source             = {src->id, uint16_t(src_index), MEDIA_PAD_FL_SOURCE, {}};
    desc.sink               = {sink->id, uint16_t(sink_index), MEDIA_PAD_FL_SINK, {}};
    desc.flags              = (link.flags & ~MEDIA_LNK_FL_ENABLED) | (enable ? MEDIA_LNK_FL_ENABLED : 0);
    ensure(ioctl(fd.as_handle(), MEDIA_IOC_SETUP_LINK, &desc) == 0);
    return true;
}

auto MediaDevice::configure_link(const LinkInfo& info, const bool enable) -> bool {
    // immutable link is treated as success
    if(info.link->flags & MEDIA_LNK_FL_IMMUTABLE) {
        return true;
    }

    auto desc   = media_link_desc();
    desc.source = {info.src_entity->id, uint16_t(info.src_pad_num), MEDIA_PAD_FL_SOURCE, {}};
    desc.sink   = {info.sink_entity->id, uint16_t(info.sink_pad_num), MEDIA_PAD_FL_SINK, {}};
    desc.flags  = (info.link->flags & ~MEDIA_LNK_FL_ENABLED) | (enable ? MEDIA_LNK_FL_ENABLED : 0);
    ensure(ioctl(fd.as_handle(), MEDIA_IOC_SETUP_LINK, &desc) == 0);
    return true;
}

auto MediaDevice::disable_all_links() -> bool {
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
                ensure(configure_link(l, false));
            }
        }
    }
    return true;
}

auto MediaDevice::enable_exclusive_link(const LinkInfo& info) -> bool {
    ensure(disable_all_links(*info.src_entity, info.src_pad_num));
    ensure(disable_all_links(*info.sink_entity, info.sink_pad_num));
    ensure(configure_link(info, true));
    return true;
}

auto MediaDevice::disable_all_links(const Entity& entity, const uint32_t pad_num) -> bool {
    const auto& pad = entity.pads[pad_num];
    for(auto& link : pad.links) {
        const auto is_src = link.src_pad_id == pad.id;

        const auto [peer_entity, peer_pad_num] = find_pad_owner_and_index(is_src ? link.sink_pad_id : link.src_pad_id);

        const auto info = is_src ? LinkInfo{
                                       .src_entity   = &entity,
                                       .sink_entity  = peer_entity,
                                       .src_pad_num  = pad_num,
                                       .sink_pad_num = uint32_t(peer_pad_num),
                                       .link         = &link,
                                   }
                                 : LinkInfo{
                                       .src_entity   = peer_entity,
                                       .sink_entity  = &entity,
                                       .src_pad_num  = uint32_t(peer_pad_num),
                                       .sink_pad_num = pad_num,
                                       .link         = &link,
                                   };
        ensure(configure_link(info, false));
    }
    return true;
}

auto MediaDevice::debug_print() const -> void {
    std::println("driver: {}", driver);
    for(auto& e : entities) {
        std::println("  entity {}: {} {}", e.id, e.name, e.dev_node);
        // continue;
        for(auto& p : e.pads) {
            std::println("    pad {}", p.id);
            for(auto& l : p.links) {
                if(l.src_pad_id == p.id) {
                    const auto& other_name = std::get<0>(find_pad_owner_and_index(l.sink_pad_id))->name;
                    std::println("      link {}: -> {}({})", l.id, l.sink_pad_id, other_name);
                } else {
                    const auto& other_name = std::get<0>(find_pad_owner_and_index(l.src_pad_id))->name;
                    std::println("      link {}: <- {}({})", l.id, l.src_pad_id, other_name);
                }
            }
        }

        if(!e.ancillary_entities.empty()) {
            std::println("    ancillary");
            for(const auto anc : e.ancillary_entities) {
                const auto other = const_cast<MediaDevice*>(this)->find_entity_by_id(anc);
                std::println("      {}({})", other->id, other->name);
            }
        }
    }
}

auto parse_device(const char* const device, const std::unordered_map<dev_t, std::string>& node_map) -> std::optional<MediaDevice> {
    auto fd = FileDescriptor(open(device, O_RDWR));
    ensure(fd.as_handle() >= 0);

    auto info = media_device_info();
    ensure(ioctl(fd.as_handle(), MEDIA_IOC_DEVICE_INFO, &info) == 0);

    auto topology = media_v2_topology();
    ensure(ioctl(fd.as_handle(), MEDIA_IOC_G_TOPOLOGY, &topology) == 0);

    auto entities   = std::vector<media_v2_entity>(topology.num_entities);
    auto interfaces = std::vector<media_v2_interface>(topology.num_interfaces);
    auto links      = std::vector<media_v2_link>(topology.num_links);
    auto pads       = std::vector<media_v2_pad>(topology.num_pads);

    topology.ptr_entities   = std::bit_cast<uintptr_t>(entities.data());
    topology.ptr_interfaces = std::bit_cast<uintptr_t>(interfaces.data());
    topology.ptr_links      = std::bit_cast<uintptr_t>(links.data());
    topology.ptr_pads       = std::bit_cast<uintptr_t>(pads.data());
    ensure(ioctl(fd.as_handle(), MEDIA_IOC_G_TOPOLOGY, &topology) == 0);

    unwrap_mut(parsed_topology, parse_topology(topology, node_map));

    return MediaDevice{device, info.driver, std::move(parsed_topology), std::move(fd)};
}
