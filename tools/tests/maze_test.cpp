#include "maze_model.hpp"
#include "modules.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <queue>
#include <vector>
using namespace el_sistema;
static MotionSample flat() {MotionSample m;m.valid=true;m.accel_z=1;return m;}
static float wall_distance2(float x,float y,const MazeRect& r) {
    const float dx=x-std::clamp(x,r.x,r.x+r.w),dy=y-std::clamp(y,r.y,r.y+r.h);return dx*dx+dy*dy;
}
static bool safe(const MazeLevel& level,float x,float y,bool avoid_holes=true) {
    if(x<18||x>MAZE_WIDTH-18||y<18||y>MAZE_HEIGHT-18) return false;
    for(unsigned i=0;i<level.wall_count;++i) if(wall_distance2(x,y,level.walls[i])<16*16) return false;
    if(avoid_holes) for(unsigned i=0;i<level.hole_count;++i) {
        const float dx=x-level.holes[i].x,dy=y-level.holes[i].y;if(dx*dx+dy*dy<20*20) return false;
    }
    return true;
}
static std::vector<MazePoint> path(const MazeLevel& level,MazePoint target,bool avoid_holes=true) {
    constexpr int cols=MAZE_WIDTH/20,rows=MAZE_HEIGHT/20;
    const int start=2+2*cols,finish=int((target.x-10)/20)+int((target.y-10)/20)*cols;
    std::vector<int> previous(cols*rows,-1);std::queue<int> queue;queue.push(start);previous[start]=start;
    const auto point=[](int i) {return MazePoint{float(10+20*(i%cols)),float(10+20*(i/cols))};};
    while(!queue.empty()&&previous[finish]<0) {
        const int from=queue.front();queue.pop();const auto p=point(from);
        for(int delta:{-1,1,-cols,cols}) {
            const int next=from+delta;
            if(next<0||next>=cols*rows||previous[next]>=0||(std::abs(delta)==1&&next/cols!=from/cols)) continue;
            const auto q=point(next);
            if(!safe(level,q.x,q.y,avoid_holes)||!safe(level,(q.x+p.x)/2,(q.y+p.y)/2,avoid_holes)) continue;
            previous[next]=from;queue.push(next);
        }
    }
    assert(previous[finish]>=0);
    std::vector<MazePoint> result;
    for(int i=finish;i!=start;i=previous[i]) result.push_back(point(i));
    std::reverse(result.begin(),result.end());return result;
}
static void calibrate(MazeEngine& engine,int64_t& now) {
    engine.start(now,flat());assert(engine.snapshot().state==MazeState::Calibrating);
    for(int i=0;i<30;++i) {now+=20000;engine.step(now,flat());}
    assert(engine.snapshot().state==MazeState::Running&&engine.snapshot().elapsed_ms==0);
}
static void drive(MazeEngine& engine,MazePoint target,int64_t& now) {
    for(int tick=0;tick<2000&&engine.snapshot().state==MazeState::Running;++tick) {
        const auto b=engine.snapshot().ball;
        const float dx=target.x-b.x,dy=target.y-b.y;
        if(dx*dx+dy*dy<1&&b.vx*b.vx+b.vy*b.vy<9) return;
        auto m=flat();m.accel_x=std::clamp(dx*.012f-b.vx*.045f,-.55f,.55f);
        m.accel_y=std::clamp(dy*.012f-b.vy*.045f,-.55f,.55f);
        now+=20000;engine.step(now,m);
    }
    assert(engine.snapshot().state!=MazeState::Running); // No stalled navigation.
}
static void levels_and_goals() {
    for(unsigned level=0;level<MAZE_LEVELS;++level) {
        const auto maze=make_maze_level(level),same=make_maze_level(level);
        assert(maze.wall_count==70&&maze.hole_count==2+level*2);
        for(unsigned i=0;i<maze.wall_count;++i) {
            assert(maze.walls[i].x==same.walls[i].x&&maze.walls[i].y==same.walls[i].y);
        }
        auto route=path(maze,maze.goal);assert(!route.empty());
        MazeEngine engine;engine.select(level);int64_t now=1000;calibrate(engine,now);
        for(auto target:route) {
            drive(engine,target,now);
            assert(engine.snapshot().state!=MazeState::Fallen&&engine.snapshot().state!=MazeState::Paused);
            if(engine.snapshot().state==MazeState::Won) break;
        }
        assert(engine.snapshot().state==MazeState::Won&&engine.snapshot().best_ms[level]>0);
        const auto finished=engine.snapshot().elapsed_ms;now+=20000;engine.step(now,flat());
        assert(engine.snapshot().elapsed_ms==finished);engine.resume(now,flat());assert(engine.snapshot().state==MazeState::Won);
        engine.calibrate(now);assert(engine.snapshot().state==MazeState::Won);
        std::printf("Laberinto %u: ruta segura, agujeros evitables y meta alcanzada en %.1f s simulados\n",level+1,finished/1000.f);
    }
}
static void physics() {
    MazeLevel free;MazeBall ball;advance_maze_ball(ball,free,0,0,.25f);assert(ball.x==50&&ball.y==50);
    for(int n=0;n<30;++n) advance_maze_ball(ball,free,.2f,0,.02f);
    assert(ball.x>50&&ball.vx>0&&ball.y==50);
    const float speed=ball.vx;for(int n=0;n<100;++n) advance_maze_ball(ball,free,0,0,.02f);
    assert(std::abs(ball.vx)<speed*.01f);
    MazeLevel wall;wall.walls[0]={200,0,8,MAZE_HEIGHT};wall.wall_count=1;ball={100,450,0,0};
    for(int n=0;n<1000;++n) {advance_maze_ball(ball,wall,.8f,0,.1f);assert(ball.x<=186.01f);}
    for(unsigned index=0;index<MAZE_LEVELS;++index) {
        const auto level=make_maze_level(index);ball={50,50,0,0};
        for(int n=0;n<5000;++n) {
            advance_maze_ball(ball,level,std::sin(n*.13f),std::cos(n*.17f),.02f);
            assert(std::isfinite(ball.x)&&std::isfinite(ball.y)&&ball.x>=18&&ball.x<=MAZE_WIDTH-18);
            assert(ball.y>=18&&ball.y<=MAZE_HEIGHT-18);
            for(unsigned w=0;w<level.wall_count;++w) assert(wall_distance2(ball.x,ball.y,level.walls[w])>=14*14-.1f);
        }
    }
}
static void lifecycle() {
    MazeEngine engine;int64_t now=1000;calibrate(engine,now);engine.step(now+=20000,flat());assert(engine.snapshot().elapsed_ms==20);
    engine.pause(now);const auto paused=engine.snapshot();engine.step(now+=5000000,flat());
    assert(engine.snapshot().elapsed_ms==paused.elapsed_ms&&engine.snapshot().ball.x==paused.ball.x);
    engine.resume(now,flat());engine.step(now+=20000,flat());assert(engine.snapshot().elapsed_ms==40);
    auto invalid=flat();invalid.valid=false;engine.step(now+=20000,invalid);
    assert(engine.snapshot().state==MazeState::Paused&&engine.snapshot().error==MazeError::Sensor);
    engine.step(now+=20000,flat());assert(engine.snapshot().state==MazeState::Paused);
    engine.resume(now,flat());engine.step(now+=1000000,flat());assert(engine.snapshot().error==MazeError::Timing);
    invalid=flat();invalid.accel_x=std::numeric_limits<float>::quiet_NaN();engine.start(now,invalid);
    assert(engine.snapshot().error==MazeError::Sensor);
    invalid=flat();invalid.accel_z=.1f;engine.start(now,invalid);assert(engine.snapshot().error==MazeError::PlaceFlat);
    engine.start(now,flat());
    for(int n=0;n<30;++n) {auto shake=flat();shake.accel_x=n%2?.5f:-.5f;engine.step(now+=20000,shake);}
    assert(engine.snapshot().error==MazeError::HoldStill);
    engine.select(0);calibrate(engine,now);const auto maze=make_maze_level(0);
    // Deliberately navigate to a hole to exercise failure/retry, not a teleport.
    auto route=path(maze,maze.holes[0],false);
    for(auto target:route) {drive(engine,target,now);if(engine.snapshot().state!=MazeState::Running) break;}
    if(engine.snapshot().state==MazeState::Running) drive(engine,maze.holes[0],now);
    assert(engine.snapshot().state==MazeState::Fallen);
    engine.resume(now,flat());assert(engine.snapshot().state==MazeState::Fallen);
    engine.start(now,flat());assert(engine.snapshot().state==MazeState::Calibrating&&engine.snapshot().elapsed_ms==0);
}
static void preferences() {
    MazeSnapshot s;s.level=2;s.best_ms={15001,99000,340005};auto raw=encode_maze_preferences(s);
    unsigned level=0;std::array<uint32_t,MAZE_LEVELS> best{};assert(decode_maze_preferences(raw,level,best));
    assert(level==2&&best==s.best_ms);raw[7]^=1;assert(!decode_maze_preferences(raw,level,best));
    MazeEngine engine;engine.preferences(level,best);assert(engine.snapshot().state==MazeState::Ready);
    assert(engine.snapshot().level==2&&engine.snapshot().best_ms==best);
    assert(kModules.size()==6&&find_module(ModuleId::Maze));
    // Launcher bounds: six modules in both compositions, including centered final rows.
    for(int columns:{3,5}) {
        const int width=columns==3?720:1280,group_width=152*columns+12*(columns-1);
        assert(group_width<width);
        for(int i=0;i<6;++i) {
            const int row=i/columns,count=std::min(columns,6-row*columns);
            const int x=(columns-count)*164/2+(i%columns)*164;assert(x>=0&&x+152<=group_width);
        }
    }
}
int main() {physics();lifecycle();levels_and_goals();preferences();std::puts("OK: paredes, calibración, pausa, sensor, agujeros, tres metas, récord y lanzador.");}
