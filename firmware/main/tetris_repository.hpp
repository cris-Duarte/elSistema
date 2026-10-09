#pragma once
#include "tetris_model.hpp"
#include "nvs.h"
namespace el_sistema {
class TetrisRepository {
public:
    ~TetrisRepository() { if(handle_) nvs_close(handle_); }
    bool init();
    bool load(TetrisSnapshot& state,bool& corrupt);
    bool save(const TetrisSnapshot& state);
private:
    nvs_handle_t handle_=0;
};
}
