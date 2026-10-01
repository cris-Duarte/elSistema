#pragma once

#include "model.hpp"

#include "nvs.h"

namespace el_sistema {

class MissionRepository {
public:
    bool init();
    bool load(AppState& state);
    bool save(const AppState& state);
    bool save_checkpoint(MissionId active_id, uint32_t elapsed_seconds, int64_t interval_started_utc);
    bool restore_checkpoint(AppState& state);
    uint8_t storage_percent() const;
    const char* last_error() const { return last_error_.c_str(); }

private:
    bool load_file(const char* path, AppState& state);
    bool write_file(const char* path, const AppState& state);
    void load_preferences(AppState& state);
    void save_preferences(const AppState& state);
    void set_error(const char* message);

    nvs_handle_t nvs_ = 0;
    bool mounted_ = false;
    std::string last_error_;
};

}  // namespace el_sistema
