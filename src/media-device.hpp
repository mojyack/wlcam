#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

#include "util/fd.hpp"

struct Link {
    uint32_t id;
    uint32_t src_pad_id;
    uint32_t sink_pad_id;
    uint32_t flags;
};

struct Pad {
    uint32_t          id;
    uint32_t          flags;
    std::vector<Link> links;
};

struct Interface {
    uint32_t major;
    uint32_t minor;
    uint32_t type;
};

struct Entity {
    uint32_t              id;
    uint32_t              function;
    Interface             interface;
    std::string           name;
    std::vector<Pad>      pads;
    std::string           dev_node;
    std::vector<uint32_t> ancillary_entities;
};

struct LinkInfo {
    const Entity* src_entity;
    const Entity* sink_entity;
    uint32_t      src_pad_num;
    uint32_t      sink_pad_num;
    const Link*   link;
};

struct MediaDevice {
    std::string         dev_node;
    std::string         driver;
    std::vector<Entity> entities;
    FileDescriptor      fd;

    auto find_entity_by_id(uint32_t id) -> Entity*;
    auto find_entity_by_name(std::string_view name) -> Entity*;
    auto find_pad_owner_and_index(uint32_t pad_id) const -> std::tuple<const Entity*, size_t>;
    auto find_links(const Entity* src, const Entity* sink) const -> std::vector<LinkInfo>;
    auto find_link(const Entity* src, const Entity* sink) const -> std::optional<LinkInfo>;
    auto configure_link(const Link& link, bool enable) -> bool;
    auto configure_link(const LinkInfo& info, bool enable) -> bool;
    auto disable_all_links() -> bool;
    auto disable_all_links(const Entity& entity, uint32_t pad_num) -> bool;
    auto enable_exclusive_link(const LinkInfo& info) -> bool;
    auto debug_print() const -> void;
};

auto parse_device(const char* const device, const std::unordered_map<dev_t, std::string>& node_map) -> std::optional<MediaDevice>;
