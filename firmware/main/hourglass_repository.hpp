#pragma once
#include "hourglass_model.hpp"
#include "nvs.h"

namespace el_sistema {
class HourglassRepository {
public:
    bool init();
    bool load(HourglassSnapshot& state, bool& corrupt);
    bool save(const HourglassSnapshot& state);
private:
    nvs_handle_t handle_ = 0;
};
} // namespace el_sistema
