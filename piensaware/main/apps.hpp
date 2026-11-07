#pragma once
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "lvgl.h"
namespace piensaware {
// The shell owns and deletes the root after close(). Apps release timers/workers in close().
struct AppDescriptor {
 std::string id, name, icon;
 std::function<void(lv_obj_t*)> open;
 std::function<void()> close;
};
class AppRegistry {
 std::vector<AppDescriptor> apps_;
public:
 bool add(AppDescriptor app) {
  if(app.id.empty()||app.name.empty()||!app.open||!app.close||
   std::any_of(apps_.begin(),apps_.end(),[&](auto&a){return a.id==app.id;})) return false;
  apps_.push_back(std::move(app)); return true;
 }
 const std::vector<AppDescriptor>& all() const {return apps_;}
};
}
