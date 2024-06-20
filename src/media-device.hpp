#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

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

struct MediaDevice {
    std::string         dev_node;
    std::string         driver;
    std::vector<Entity> entities;
    int                 fd;

    auto find_entity_by_id(uint32_t id) -> Entity*;
    auto find_entity_by_name(std::string_view name) -> Entity*;
    auto find_pad_owner_and_index(uint32_t pad_id) const -> std::tuple<const Entity*, size_t>;
    auto configure_link(Link& link, bool enable) -> void;
    auto disable_all_links() -> void;
    auto debug_print() const -> void;
};

auto parse_device(const char* const device, const std::unordered_map<dev_t, std::string>& node_map) -> MediaDevice;
