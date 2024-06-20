#pragma once
#include "../media-device.hpp"
#include "../util/fd.hpp"

namespace ipu3 {
struct ImgUDevice {
    FileDescriptor imgu;
    uint32_t       imgu_input_pad_index      = UINT_MAX;
    uint32_t       imgu_output_pad_index     = UINT_MAX;
    uint32_t       imgu_parameters_pad_index = UINT_MAX;
    uint32_t       imgu_viewfinder_pad_index = UINT_MAX;
    uint32_t       imgu_stat_pad_index       = UINT_MAX;

    FileDescriptor input;
    FileDescriptor output;
    FileDescriptor parameters;
    FileDescriptor viewfinder;
    FileDescriptor stat;

    auto init(MediaDevice& media, std::string_view entity_name) -> void;
};
} // namespace ipu3
