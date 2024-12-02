#include <fcntl.h>
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../macros/assert.hpp"
#include "imgu.hpp"

namespace ipu3 {
auto ImgUDevice::init(MediaDevice& media, const std::string_view entity_name) -> bool {
    auto& imgu = *media.find_entity_by_name(entity_name);
    this->imgu = FileDescriptor(open(imgu.dev_node.data(), O_RDWR));
    ensure(this->imgu.as_handle() >= 0);

    auto& input = *media.find_entity_by_name(build_string(entity_name, " input"));
    this->input = FileDescriptor(open(input.dev_node.data(), O_RDWR));
    ensure(this->input.as_handle() >= 0);

    auto& output = *media.find_entity_by_name(build_string(entity_name, " output"));
    this->output = FileDescriptor(open(output.dev_node.data(), O_RDWR));
    ensure(this->output.as_handle() >= 0);

    auto& parameters = *media.find_entity_by_name(build_string(entity_name, " parameters"));
    this->parameters = FileDescriptor(open(parameters.dev_node.data(), O_RDWR));
    ensure(this->parameters.as_handle() >= 0);

    auto& viewfinder = *media.find_entity_by_name(build_string(entity_name, " viewfinder"));
    this->viewfinder = FileDescriptor(open(viewfinder.dev_node.data(), O_RDWR));
    ensure(this->viewfinder.as_handle() >= 0);

    auto& stat = *media.find_entity_by_name(build_string(entity_name, " 3a stat"));
    this->stat = FileDescriptor(open(stat.dev_node.data(), O_RDWR));
    ensure(this->stat.as_handle() >= 0);

    // find pad numbers
    for(auto i = 0u; i < imgu.pads.size(); i += 1) {
        const auto& p = imgu.pads[i];
        for(const auto& l : p.links) {
            if(l.src_pad_id == p.id) {
                const auto [sink, _] = media.find_pad_owner_and_index(l.sink_pad_id);
                if(sink->id == output.id) {
                    imgu_output_pad_index = i;
                } else if(sink->id == viewfinder.id) {
                    imgu_viewfinder_pad_index = i;
                } else if(sink->id == stat.id) {
                    imgu_stat_pad_index = i;
                }
            } else {
                const auto [src, _] = media.find_pad_owner_and_index(l.src_pad_id);
                if(src->id == input.id) {
                    imgu_input_pad_index = i;
                } else if(src->id == parameters.id) {
                    imgu_parameters_pad_index = i;
                }
            }
        }
    }
    ensure(imgu_input_pad_index != UINT_MAX);
    ensure(imgu_output_pad_index != UINT_MAX);
    ensure(imgu_parameters_pad_index != UINT_MAX);
    ensure(imgu_viewfinder_pad_index != UINT_MAX);
    ensure(imgu_stat_pad_index != UINT_MAX);

    // enable imgu <- input
    ensure(media.configure_link(imgu.pads[imgu_input_pad_index].links[0], true));
    // enable imgu -> output
    ensure(media.configure_link(imgu.pads[imgu_output_pad_index].links[0], true));
    // enable imgu -> viewfinder
    ensure(media.configure_link(imgu.pads[imgu_viewfinder_pad_index].links[0], true));
    // enable imgu <- parameters
    ensure(media.configure_link(imgu.pads[imgu_parameters_pad_index].links[0], true));
    // enable imgu -> stat
    ensure(media.configure_link(imgu.pads[imgu_stat_pad_index].links[0], true));

    return true;
}
} // namespace ipu3
