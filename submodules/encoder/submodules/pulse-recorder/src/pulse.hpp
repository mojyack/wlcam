#pragma once
#include <optional>
#include <string>
#include <vector>

#include <pulse/error.h>
#include <pulse/simple.h>

#include "macros/autoptr.hpp"

namespace pa {
declare_autoptr(PASimple, pa_simple, pa_simple_free);

struct Params {
    pa_sample_format_t sample_format;
    uint32_t           sample_rate;
    uint32_t           samples_per_read;
    std::string        client_name = "recorder";
    std::string        device      = "";
};

class Recorder {
  private:
    Params       params;
    AutoPASimple pa;
    uint8_t      sample_format_size;

  public:
    auto init(Params params) -> bool;
    auto read_buffer() -> std::optional<std::vector<std::byte>>;
};
} // namespace pa
