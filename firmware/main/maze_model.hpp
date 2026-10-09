#pragma once
#include "home_scene.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {
inline constexpr int MAZE_COLUMNS=6, MAZE_ROWS=9, MAZE_CELL=100, MAZE_LEVELS=3;
inline constexpr int MAZE_WIDTH=MAZE_COLUMNS*MAZE_CELL, MAZE_HEIGHT=MAZE_ROWS*MAZE_CELL;
inline constexpr float MAZE_BALL_RADIUS=14.f;
struct MazeRect { float x=0,y=0,w=0,h=0; };
struct MazePoint { float x=0,y=0; };
struct MazeBall { float x=50,y=50,vx=0,vy=0; };
struct MazeLevel {
    std::array<MazeRect,123> walls{};
    std::array<MazePoint,6> holes{};
    unsigned wall_count=0,hole_count=0;
    MazePoint start{50,50},goal{550,850};
};
MazeLevel make_maze_level(unsigned level);
// Bounded circle/rectangle physics; no LVGL, hardware or storage dependencies.
void advance_maze_ball(MazeBall& ball,const MazeLevel& level,float tilt_x,float tilt_y,float seconds);
enum class MazeState : uint8_t { Ready,Calibrating,Running,Paused,Fallen,Won };
enum class MazeError : uint8_t { None,Sensor,PlaceFlat,HoldStill,Timing,Unavailable };
struct MazeSnapshot {
    MazeState state=MazeState::Ready;
    MazeError error=MazeError::None;
    MazeBall ball{};
    unsigned level=0,attempts=0;
    uint32_t elapsed_ms=0,generation=0;
    std::array<uint32_t,MAZE_LEVELS> best_ms{};
    bool available=true,storage_error=false;
};
class MazeEngine {
public:
    const MazeSnapshot& snapshot() const { return state_; }
    void select(unsigned level);
    void start(int64_t now,const MotionSample& motion);
    void calibrate(int64_t now);
    void pause(int64_t now);
    void resume(int64_t now,const MotionSample& motion);
    void step(int64_t now,const MotionSample& motion);
    void preferences(unsigned level,const std::array<uint32_t,MAZE_LEVELS>& best);
private:
    bool usable(const MotionSample& motion) const;
    void fail(MazeError error);
    MazeSnapshot state_{};
    MazeLevel level_=make_maze_level(0);
    int64_t last_us_=0,elapsed_us_=0,calibration_since_=0;
    float bias_x_=0,bias_y_=0,filtered_x_=0,filtered_y_=0;
    float sum_x_=0,sum_y_=0,squared_x_=0,squared_y_=0;
    unsigned samples_=0;
    bool calibrated_=false;
};
using MazePreferences=std::array<uint8_t,24>;
MazePreferences encode_maze_preferences(const MazeSnapshot& snapshot);
bool decode_maze_preferences(const MazePreferences& record,unsigned& level,std::array<uint32_t,MAZE_LEVELS>& best);
const char* maze_error_text(MazeError error);
} // namespace el_sistema
