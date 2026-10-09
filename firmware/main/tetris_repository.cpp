#include "tetris_repository.hpp"
#include <algorithm>
namespace el_sistema {
bool TetrisRepository::init() { return handle_||nvs_open("tetris",NVS_READWRITE,&handle_)==ESP_OK; }
bool TetrisRepository::load(TetrisSnapshot& state,bool& corrupt) {
    corrupt=false;if(!handle_) return false;
    TetrisPreferences prefs{};size_t n=prefs.size();
    auto err=nvs_get_blob(handle_,"preferences",prefs.data(),&n);
    if(err!=ESP_ERR_NVS_NOT_FOUND&&(err!=ESP_OK||n!=prefs.size()||!decode_tetris_preferences(prefs,state))) corrupt=true;
    TetrisSnapshot session{};TetrisRecord record{};n=record.size();
    err=nvs_get_blob(handle_,"session",record.data(),&n);
    if(err!=ESP_ERR_NVS_NOT_FOUND) {
        if(err!=ESP_OK||!decode_tetris(record.data(),n,session)) corrupt=true;
        else {
            session.record=std::max(state.record,session.score);session.music=state.music;
            session.effects=state.effects;session.volume=state.volume;state=session;
        }
    }
    return true;
}
bool TetrisRepository::save(const TetrisSnapshot& state) {
    if(!handle_) return false;
    auto record=encode_tetris(state);auto prefs=encode_tetris_preferences(state);
    return nvs_set_blob(handle_,"session",record.data(),record.size())==ESP_OK&&
           nvs_set_blob(handle_,"preferences",prefs.data(),prefs.size())==ESP_OK&&nvs_commit(handle_)==ESP_OK;
}
}
