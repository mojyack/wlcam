#include <memory>

#include "../graphics/bayer.hpp"
#include "../ui.hpp"

namespace {
struct DebayerSlider : Slider {
    std::string name;
    float*      dest;
    uint16_t    min;
    uint16_t    max;
    uint16_t    denom;

    auto get_range() const -> std::array<int, 2> override {
        return {min, max};
    }

    auto get_value() const -> int override {
        return *dest * denom;
    }

    auto set_value(int value) -> void override {
        *dest = 1. * value / denom;
    }
};

struct DebayerSliderButton : Button {
    DebayerSlider slider;

    auto get_label() -> std::string_view override {
        return slider.name;
    }

    auto on_pressed() -> void override {
        if(expand.get_index() == Expand::index_of<Slider*>) {
            expand.reset();
            pressed = false;
        } else {
            expand.emplace<Slider*>(&slider);
        }
    }
};

auto make_button(std::string name, float& dest, uint16_t min, uint16_t max, uint16_t denom) -> std::unique_ptr<DebayerSliderButton> {
    auto button          = std::make_unique<DebayerSliderButton>();
    button->slider.name  = std::move(name);
    button->slider.dest  = &dest;
    button->slider.min   = min;
    button->slider.max   = max;
    button->slider.denom = denom;
    return button;
}
} // namespace

auto add_debayer_param_buttons(std::vector<std::unique_ptr<Button>>& buttons) -> void {
    buttons.emplace_back(make_button("Gamma", bayer_params.gamma, 10, 300, 100));
    buttons.emplace_back(make_button("Black Level", bayer_params.black_level, 0, 255, 255));
    buttons.emplace_back(make_button("WB R", bayer_params.wb_gain[0], 0, 400, 100));
    buttons.emplace_back(make_button("WB G", bayer_params.wb_gain[1], 0, 400, 100));
    buttons.emplace_back(make_button("WB B", bayer_params.wb_gain[2], 0, 400, 100));
    buttons.emplace_back(make_button("LSC", bayer_params.lsc, 0, 200, 100));
}
