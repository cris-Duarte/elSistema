#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <array>
#include <algorithm>
namespace piensaware {
struct Preferences {int brightness=72;bool reduced_motion=false,writable=true;bool save()const{return true;}};
struct Network {std::string ssid;int rssi=0;bool secured=true;};
struct WifiSnapshot {std::string status="Sin conexión",ssid;std::vector<Network> networks;std::vector<std::string> saved;bool busy=false;unsigned revision=0;};
struct WifiService {WifiSnapshot state;WifiSnapshot snapshot(){return state;}bool scan(){return true;}bool connect_known(const std::string&){return true;}bool forget(const std::string&){return true;}bool connect(const std::string&,const std::string&){return true;}};
struct ClockService {bool is_valid()const{return true;}int64_t now_utc()const{return 1794076200;}std::string format_status(int64_t,int)const{return "16:30  07/11/2026";}bool set_local(int,int,int,int,int,int){return true;}};
struct OrientationService {int value=-1;int poll(){return value;}void set_current(int){};};
enum class BatteryState {Unavailable,Missing,Available};
struct PowerStatus {BatteryState state=BatteryState::Available;int battery_pct=76;};
struct PowerService {PowerStatus poll(){return {};}};
}
