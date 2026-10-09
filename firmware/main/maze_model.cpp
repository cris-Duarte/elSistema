#include "maze_model.hpp"
#include <algorithm>
#include <cmath>

namespace el_sistema {
namespace {
uint32_t random_next(uint32_t& seed) {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
uint32_t crc(const uint8_t* bytes,size_t count) {
    uint32_t result=0xffffffff;
    for(size_t i=0;i<count;++i) {result^=bytes[i];for(int n=0;n<8;++n) result=(result>>1)^((result&1)?0xedb88320:0);}
    return ~result;
}
void put(uint8_t* p,uint32_t value) {for(int n=0;n<4;++n) p[n]=uint8_t(value>>(8*n));}
uint32_t get(const uint8_t* p) {uint32_t result=0;for(int n=0;n<4;++n) result|=uint32_t(p[n])<<(8*n);return result;}
void collide(MazeBall& b,const MazeRect& r) {
    const float px=std::clamp(b.x,r.x,r.x+r.w),py=std::clamp(b.y,r.y,r.y+r.h);
    float nx=b.x-px,ny=b.y-py;const float d2=nx*nx+ny*ny;
    if(d2>=MAZE_BALL_RADIUS*MAZE_BALL_RADIUS) return;
    float penetration;
    if(d2>0.00001f) {const float d=std::sqrt(d2);nx/=d;ny/=d;penetration=MAZE_BALL_RADIUS-d;}
    else {
        const float left=b.x-r.x,right=r.x+r.w-b.x,top=b.y-r.y,bottom=r.y+r.h-b.y;
        const float edge=std::min({left,right,top,bottom});nx=ny=0;
        if(edge==left) nx=-1;else if(edge==right) nx=1;else if(edge==top) ny=-1;else ny=1;
        penetration=MAZE_BALL_RADIUS+edge;
    }
    b.x+=nx*penetration;b.y+=ny*penetration;
    const float speed=b.vx*nx+b.vy*ny;
    if(speed<0) {b.vx-=1.18f*speed*nx;b.vy-=1.18f*speed*ny;}
}
}
MazeLevel make_maze_level(unsigned level) {
    level=std::min<unsigned>(level,MAZE_LEVELS-1);
    MazeLevel result;
    constexpr unsigned count=MAZE_COLUMNS*MAZE_ROWS;
    std::array<uint8_t,count> closed{};closed.fill(15);
    std::array<bool,count> visited{};
    std::array<unsigned,count> stack{};unsigned depth=1;visited[0]=true;
    uint32_t seed=0x517cc1b7u+level*0x1f123bb5u;
    constexpr int dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    while(depth) {
        const unsigned cell=stack[depth-1];const int x=cell%MAZE_COLUMNS,y=cell/MAZE_COLUMNS;
        std::array<int,4> choices{};unsigned n=0;
        for(int d=0;d<4;++d) {
            const int xx=x+dx[d],yy=y+dy[d];
            if(xx>=0&&xx<MAZE_COLUMNS&&yy>=0&&yy<MAZE_ROWS&&!visited[yy*MAZE_COLUMNS+xx]) choices[n++]=d;
        }
        if(!n) {--depth;continue;}
        const int d=choices[random_next(seed)%n];const unsigned next=(y+dy[d])*MAZE_COLUMNS+x+dx[d];
        closed[cell]&=uint8_t(~(1u<<d));closed[next]&=uint8_t(~(1u<<((d+2)%4)));
        visited[next]=true;stack[depth++]=next;
    }
    const auto wall=[&](float x,float y,float w,float h) {result.walls[result.wall_count++]={x,y,w,h};};
    for(int y=0;y<MAZE_ROWS;++y) for(int x=0;x<MAZE_COLUMNS;++x) {
        const uint8_t sides=closed[y*MAZE_COLUMNS+x];
        if(sides&8) wall(x*MAZE_CELL-4.f,y*MAZE_CELL-4.f,8,MAZE_CELL+8);
        if(sides&1) wall(x*MAZE_CELL-4.f,y*MAZE_CELL-4.f,MAZE_CELL+8,8);
        if(x==MAZE_COLUMNS-1) wall(MAZE_WIDTH-4.f,y*MAZE_CELL-4.f,8,MAZE_CELL+8);
        if(y==MAZE_ROWS-1) wall(x*MAZE_CELL-4.f,MAZE_HEIGHT-4.f,MAZE_CELL+8,8);
    }
    std::array<bool,count> used{};
    result.hole_count=2+level*2;
    for(unsigned i=0;i<result.hole_count;++i) {
        unsigned cell;
        do {cell=2+random_next(seed)%(count-4);} while(used[cell]);
        used[cell]=true;
        result.holes[i]={float((cell%MAZE_COLUMNS)*MAZE_CELL+50)+(i%2?20.f:-20.f),
                         float((cell/MAZE_COLUMNS)*MAZE_CELL+50)+12.f};
    }
    return result;
}
void advance_maze_ball(MazeBall& b,const MazeLevel& level,float x,float y,float seconds) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(seconds)||seconds<=0) return;
    seconds=std::min(seconds,0.25f);
    const int steps=std::max(1,int(std::ceil(seconds/0.004f)));const float dt=seconds/steps;
    for(int n=0;n<steps;++n) {
        b.vx+=std::clamp(x,-0.8f,0.8f)*700.f*dt;b.vy+=std::clamp(y,-0.8f,0.8f)*700.f*dt;
        const float damping=std::exp(-2.5f*dt);b.vx*=damping;b.vy*=damping;
        const float speed=std::sqrt(b.vx*b.vx+b.vy*b.vy);
        if(speed>220) {b.vx*=220/speed;b.vy*=220/speed;}
        b.x+=b.vx*dt;b.y+=b.vy*dt;
        // Multiple contacts resolve corners without tunneling through thin walls.
        for(int pass=0;pass<3;++pass) for(unsigned w=0;w<level.wall_count;++w) collide(b,level.walls[w]);
        b.x=std::clamp(b.x,18.f,MAZE_WIDTH-18.f);b.y=std::clamp(b.y,18.f,MAZE_HEIGHT-18.f);
    }
}
bool MazeEngine::usable(const MotionSample& m) const {
    return m.valid&&std::isfinite(m.accel_x)&&std::isfinite(m.accel_y)&&std::isfinite(m.accel_z);
}
void MazeEngine::fail(MazeError error) {
    state_.state=MazeState::Paused;state_.error=error;state_.ball.vx=state_.ball.vy=0;++state_.generation;
}
void MazeEngine::select(unsigned level) {
    const auto best=state_.best_ms;state_={};state_.best_ms=best;
    state_.level=std::min<unsigned>(level,MAZE_LEVELS-1);level_=make_maze_level(state_.level);
    state_.ball={level_.start.x,level_.start.y,0,0};elapsed_us_=0;calibrated_=false;
}
void MazeEngine::preferences(unsigned level,const std::array<uint32_t,MAZE_LEVELS>& best) {select(level);state_.best_ms=best;}
void MazeEngine::start(int64_t now,const MotionSample& motion) {
    state_.state=MazeState::Ready;
    state_.ball={level_.start.x,level_.start.y,0,0};state_.elapsed_ms=0;elapsed_us_=0;++state_.attempts;
    calibrate(now);
    if(!usable(motion)) fail(MazeError::Sensor);
    else if(std::abs(motion.accel_z)<0.7f) fail(MazeError::PlaceFlat);
}
void MazeEngine::calibrate(int64_t now) {
    if(state_.state==MazeState::Fallen||state_.state==MazeState::Won) return;
    state_.state=MazeState::Calibrating;state_.error=MazeError::None;calibrated_=false;
    sum_x_=sum_y_=squared_x_=squared_y_=0;samples_=0;last_us_=calibration_since_=now;
    state_.ball.vx=state_.ball.vy=filtered_x_=filtered_y_=0;++state_.generation;
}
void MazeEngine::pause(int64_t now) {
    if(state_.state==MazeState::Running||state_.state==MazeState::Calibrating) {
        state_.state=MazeState::Paused;state_.ball.vx=state_.ball.vy=0;++state_.generation;
    }
    last_us_=now;
}
void MazeEngine::resume(int64_t now,const MotionSample& motion) {
    if(state_.state!=MazeState::Paused) return;
    if(!usable(motion)) {fail(MazeError::Sensor);return;}
    if(!calibrated_) {calibrate(now);return;}
    state_.state=MazeState::Running;state_.error=MazeError::None;last_us_=now;++state_.generation;
}
void MazeEngine::step(int64_t now,const MotionSample& motion) {
    const int64_t dt=now-last_us_;last_us_=now;
    if(state_.state!=MazeState::Running&&state_.state!=MazeState::Calibrating) return;
    if(!usable(motion)) {fail(MazeError::Sensor);return;}
    if(dt<=0) return;
    if(dt>250000) {fail(MazeError::Timing);return;}
    if(state_.state==MazeState::Calibrating) {
        if(std::abs(motion.accel_z)<0.7f) {fail(MazeError::PlaceFlat);return;}
        sum_x_+=motion.accel_x;sum_y_+=motion.accel_y;
        squared_x_+=motion.accel_x*motion.accel_x;squared_y_+=motion.accel_y*motion.accel_y;++samples_;
        if(now-calibration_since_>=600000&&samples_>=20) {
            const float x=sum_x_/samples_,y=sum_y_/samples_;
            if(squared_x_/samples_-x*x>0.0036f||squared_y_/samples_-y*y>0.0036f) {fail(MazeError::HoldStill);return;}
            bias_x_=x;bias_y_=y;calibrated_=true;state_.state=MazeState::Running;++state_.generation;
        }
        return;
    }
    elapsed_us_+=dt;state_.elapsed_ms=uint32_t(std::min<int64_t>(UINT32_MAX,elapsed_us_/1000));
    const float smoothing=1-std::exp(-float(dt)/60000.f);
    filtered_x_+=(motion.accel_x-bias_x_-filtered_x_)*smoothing;
    filtered_y_+=(motion.accel_y-bias_y_-filtered_y_)*smoothing;
    // Check hazards at every bounded step, not just the final frame position.
    int64_t remaining=dt;
    while(remaining>0&&state_.state==MazeState::Running) {
        const int64_t part=std::min<int64_t>(remaining,4000);remaining-=part;
        advance_maze_ball(state_.ball,level_,filtered_x_,filtered_y_,part/1000000.f);
        for(unsigned h=0;h<level_.hole_count;++h) {
            const float x=state_.ball.x-level_.holes[h].x,y=state_.ball.y-level_.holes[h].y;
            if(x*x+y*y<14*14) {state_.state=MazeState::Fallen;++state_.generation;break;}
        }
        const float x=state_.ball.x-level_.goal.x,y=state_.ball.y-level_.goal.y;
        if(state_.state==MazeState::Running&&x*x+y*y<24*24) {
            state_.state=MazeState::Won;
            auto& best=state_.best_ms[state_.level];if(!best||state_.elapsed_ms<best) best=state_.elapsed_ms;
            ++state_.generation;
        }
    }
}
MazePreferences encode_maze_preferences(const MazeSnapshot& s) {
    MazePreferences p{};p[0]='M';p[1]='Z';p[2]=1;p[3]=uint8_t(s.level);
    for(int n=0;n<MAZE_LEVELS;++n) put(p.data()+4+n*4,s.best_ms[n]);
    put(p.data()+20,crc(p.data(),20));return p;
}
bool decode_maze_preferences(const MazePreferences& p,unsigned& level,std::array<uint32_t,MAZE_LEVELS>& best) {
    if(p[0]!='M'||p[1]!='Z'||p[2]!=1||p[3]>=MAZE_LEVELS||get(p.data()+20)!=crc(p.data(),20)) return false;
    level=p[3];for(int n=0;n<MAZE_LEVELS;++n) best[n]=get(p.data()+4+n*4);return true;
}
const char* maze_error_text(MazeError error) {
    switch(error) {
        case MazeError::Sensor:return "IMU no disponible. Reanuda cuando se recupere.";
        case MazeError::PlaceFlat:return "Coloca la Tab5 casi horizontal para calibrar.";
        case MazeError::HoldStill:return "Mantén la Tab5 quieta al calibrar.";
        case MazeError::Timing:return "Juego pausado por una demora del sistema.";
        case MazeError::Unavailable:return "No se pudo iniciar el juego.";
        default:return "";
    }
}
} // namespace el_sistema
