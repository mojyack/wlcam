#pragma once
#include <limits>

#include "../media-device.hpp"
#include "../util/fd.hpp"

namespace ipu3 {
struct ImgUDevice {
    FileDescriptor imgu;
    uint32_t       imgu_input_pad_index      = -1;
    uint32_t       imgu_output_pad_index     = -1;
    uint32_t       imgu_parameters_pad_index = -1;
    uint32_t       imgu_viewfinder_pad_index = -1;
    uint32_t       imgu_stat_pad_index       = -1;

    FileDescriptor input;
    FileDescriptor output;
    FileDescriptor parameters;
    FileDescriptor viewfinder;
    FileDescriptor stat;

    auto init(MediaDevice& media, std::string_view entity_name) -> bool;
};
} // namespace ipu3
