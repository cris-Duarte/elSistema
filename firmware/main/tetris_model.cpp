#include "tetris_model.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace el_sistema {
namespace {
// Explicit 4x4 rotations, no wall kicks. The square never changes position.
constexpr uint16_t shapes[7][4]={
    {0x00f0,0x4444,0x0f00,0x2222}, // I
    {0x0066,0x0066,0x0066,0x0066}, // O
    {0x0072,0x0262,0x0270,0x0232}, // T
    {0x0036,0x0462,0x0360,0x0231}, // S
    {0x0063,0x0264,0x0630,0x0132}, // Z
    {0x0071,0x0226,0x0470,0x0322}, // J
    {0x0074,0x0622,0x0170,0x0223}  // L
};
uint32_t add_score(uint32_t a,uint32_t b) { return a>UINT32_MAX-b?UINT32_MAX:a+b; }
uint32_t checksum(const uint8_t* p,size_t n) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<n;++i) { crc^=p[i]; for(int j=0;j<8;++j) crc=(crc>>1)^((crc&1)?0xedb88320:0); }
    return ~crc;
}
void put(uint8_t* p,uint32_t v) { for(int i=0;i<4;++i) p[i]=uint8_t(v>>(8*i)); }
uint32_t get(const uint8_t* p) { uint32_t v=0; for(int i=0;i<4;++i) v|=uint32_t(p[i])<<(8*i); return v; }
bool fits_state(const TetrisSnapshot& s,const TetrisPiece& p) {
    for(auto c:tetris_cells(p)) if(c.x<0||c.x>=s.columns||c.y<0||c.y>=TETRIS_ROWS||s.board[c.y*s.columns+c.x]) return false;
    return true;
}
}
std::array<TetrisCell,4> tetris_cells(const TetrisPiece& p) {
    std::array<TetrisCell,4> result{}; int n=0;
    const auto mask=shapes[p.kind%7][p.rotation%4];
    for(int i=0;i<16;++i) if(mask&(1u<<i)) result[n++]={p.x+i%4,p.y+i/4};
    return result;
}
uint32_t tetris_fall_interval(uint32_t level) {
    return uint32_t(std::max(80.0,800.0*std::pow(.85,std::min<uint32_t>(level?level-1:0,100))));
}
uint32_t tetris_line_score(unsigned count,uint32_t level) {
    constexpr uint32_t base[]={0,40,100,300,1200};
    return uint32_t(std::min<uint64_t>(UINT32_MAX,uint64_t(base[std::min(count,4u)])*level));
}
bool validate_tetris(const TetrisSnapshot& s) {
    if((s.columns!=10&&s.columns!=TETRIS_COLUMNS)||uint8_t(s.state)>3||s.piece.kind>=7||s.piece.rotation>=4||s.next>=7||s.random==0||
       s.volume>100||s.lines>1000000||s.level!=1+s.lines/10||s.fall_ms>800||s.clear_ms>200||
       (s.clear_mask>>22)||bool(s.clear_mask)!=bool(s.clear_ms)) return false;
    for(auto cell:s.board) if(cell>7) return false;
    for(size_t i=s.columns*TETRIS_ROWS;i<s.board.size();++i) if(s.board[i]) return false;
    if(s.clear_mask) {
        unsigned count=0;
        for(int y=0;y<22;++y) if(s.clear_mask&(1u<<y)) {
            ++count; for(int x=0;x<s.columns;++x) if(!s.board[y*s.columns+x]) return false;
        }
        if(count>4) return false;
    } else if((s.state==TetrisState::Running||s.state==TetrisState::Paused)&&!fits_state(s,s.piece)) return false;
    return true;
}
uint8_t TetrisEngine::random_piece() {
    // Rejection sampling avoids modulo bias while preserving independent draws.
    uint32_t v;
    do { v=state_.random; v^=v<<13; v^=v>>17; v^=v<<5; state_.random=v; } while(v>UINT32_MAX-UINT32_MAX%7);
    return uint8_t((v-1)%7);
}
void TetrisEngine::start(uint32_t seed) {
    const auto old=state_; state_={}; state_.record=old.record;
    state_.music=old.music;state_.effects=old.effects;state_.volume=old.volume;
    state_.generation=old.generation+1; state_.random=seed?seed:1;
    state_.state=TetrisState::Running;state_.next=random_piece();soft_=false;sounds_=0;spawn();
}
bool TetrisEngine::restore(const TetrisSnapshot& s) {
    if(!validate_tetris(s)) return false;
    state_=s;if(state_.state==TetrisState::Running) state_.state=TetrisState::Paused;
    soft_=false;sounds_=0;changed();return true;
}
void TetrisEngine::pause() { if(state_.state==TetrisState::Running) {state_.state=TetrisState::Paused;soft_=false;changed();} sounds_=0; }
void TetrisEngine::resume() { if(state_.state==TetrisState::Paused) {state_.state=TetrisState::Running;changed();} }
void TetrisEngine::set_soft_drop(bool enabled) {
    const bool next=enabled&&state_.state==TetrisState::Running&&!state_.clear_ms;
    if(next==soft_) return;
    soft_=next;
    // Time accrued at normal gravity must never become a burst of accelerated
    // steps (effectively an unintended hard drop) when the finger moves down.
    state_.fall_ms=0;changed();
}
bool TetrisEngine::fits(const TetrisPiece& p) const { return fits_state(state_,p); }
void TetrisEngine::spawn() {
    state_.piece={state_.next,0,int8_t((state_.columns-4)/2),0};state_.next=random_piece();state_.fall_ms=0;
    if(!fits(state_.piece)) {state_.state=TetrisState::Finished;soft_=false;sounds_|=TetrisEnd;}
    changed();
}
bool TetrisEngine::move(int columns) {
    if(state_.state!=TetrisState::Running||state_.clear_ms) return false;
    columns=std::clamp(columns,-int(state_.columns),int(state_.columns));
    bool moved=false;
    for(int i=0;i<std::min(int(state_.columns),std::abs(columns));++i) {
        auto p=state_.piece;p.x+=columns<0?-1:1;
        if(!fits(p)) break;
        state_.piece=p;moved=true;
    }
    if(moved) {sounds_|=TetrisMove;changed();} return moved;
}
bool TetrisEngine::rotate() {
    if(state_.state!=TetrisState::Running||state_.clear_ms) return false;
    auto p=state_.piece;p.rotation=(p.rotation+1)%4;
    if(!fits(p)) return false;
    state_.piece=p;sounds_|=TetrisRotate;changed();return true;
}
bool TetrisEngine::descend() {
    auto p=state_.piece;++p.y;
    if(!fits(p)) {lock();return false;}
    state_.piece=p;
    if(soft_) state_.score=add_score(state_.score,1);
    state_.record=std::max(state_.record,state_.score);changed();return true;
}
void TetrisEngine::lock() {
    for(auto c:tetris_cells(state_.piece)) state_.board[c.y*state_.columns+c.x]=state_.piece.kind+1;
    sounds_|=TetrisLock;soft_=false;state_.clear_mask=0;
    for(int y=0;y<22;++y) {
        bool full=true;for(int x=0;x<state_.columns;++x) full=full&&state_.board[y*state_.columns+x];
        if(full) state_.clear_mask|=1u<<y;
    }
    if(state_.clear_mask) {state_.clear_ms=200;sounds_|=TetrisLines;}
    else finish_clear();
    changed();
}
void TetrisEngine::finish_clear() {
    const uint32_t mask=state_.clear_mask;unsigned count=0;int dst=21;
    for(int src=21;src>=0;--src) {
        if(mask&(1u<<src)) {++count;continue;}
        for(int x=0;x<state_.columns;++x) state_.board[dst*state_.columns+x]=state_.board[src*state_.columns+x];
        --dst;
    }
    while(dst>=0) {for(int x=0;x<state_.columns;++x) state_.board[dst*state_.columns+x]=0;--dst;}
    state_.score=add_score(state_.score,tetris_line_score(count,state_.level));
    state_.record=std::max(state_.record,state_.score);state_.lines+=count;
    const auto level=1+state_.lines/10;if(level!=state_.level) sounds_|=TetrisLevel;
    state_.level=level;state_.clear_ms=state_.clear_mask=0;
    bool top=false;for(int i=0;i<TETRIS_HIDDEN*state_.columns;++i) top=top||state_.board[i];
    if(top) {state_.state=TetrisState::Finished;sounds_|=TetrisEnd;soft_=false;changed();}
    else spawn();
}
void TetrisEngine::advance(uint32_t ms) {
    if(state_.state!=TetrisState::Running) return;
    // Caller subdivides elapsed monotonic time; bounds guard pathological stalls.
    ms=std::min<uint32_t>(ms,1000);
    if(state_.clear_ms) {
        if(ms>=state_.clear_ms) finish_clear();else {state_.clear_ms-=ms;changed();}
        return;
    }
    state_.fall_ms+=ms;const auto interval=soft_?40u:tetris_fall_interval(state_.level);
    while(state_.fall_ms>=interval&&state_.state==TetrisState::Running&&!state_.clear_ms) {
        state_.fall_ms-=interval;if(!descend()) break;
    }
}
void TetrisEngine::settings(bool music,bool effects,uint8_t volume) {
    state_.music=music;state_.effects=effects;state_.volume=std::min<uint8_t>(100,volume);changed();
}
TetrisRecord encode_tetris(const TetrisSnapshot& s) {
    TetrisRecord r{};r[0]='T';r[1]='E';r[2]='T';r[3]=2;r[4]=s.columns;r[5]=TETRIS_ROWS;r[6]=TETRIS_HIDDEN;
    std::copy(s.board.begin(),s.board.end(),r.begin()+8);
    constexpr size_t m=8+TETRIS_BOARD_CELLS;
    r[m]=s.piece.kind;r[m+1]=s.piece.rotation;r[m+2]=uint8_t(s.piece.x);r[m+3]=uint8_t(s.piece.y);
    r[m+4]=s.next;r[m+5]=uint8_t(s.state);
    put(r.data()+m+8,s.score);put(r.data()+m+12,s.lines);put(r.data()+m+16,s.level);
    put(r.data()+m+20,s.random);put(r.data()+m+24,s.fall_ms);put(r.data()+m+28,s.clear_ms);put(r.data()+m+32,s.clear_mask);
    put(r.data()+r.size()-4,checksum(r.data(),r.size()-4));return r;
}
bool decode_tetris(const TetrisRecord& r,TetrisSnapshot& out) {
    return decode_tetris(r.data(),r.size(),out);
}
bool decode_tetris(const uint8_t* r,size_t size,TetrisSnapshot& out) {
    if(!r||size<8||r[0]!='T'||r[1]!='E'||r[2]!='T') return false;
    const bool legacy=r[3]==1&&size==280;
    const bool current=r[3]==2&&size==TetrisRecord{}.size();
    if((!legacy&&!current)||get(r+size-4)!=checksum(r,size-4)) return false;
    TetrisSnapshot s{};
    if(legacy) {
        s.columns=10;std::copy(r+4,r+224,s.board.begin());
    } else {
        if(r[5]!=TETRIS_ROWS||r[6]!=TETRIS_HIDDEN) return false;
        s.columns=r[4];std::copy(r+8,r+8+TETRIS_BOARD_CELLS,s.board.begin());
    }
    const size_t m=legacy?224:8+TETRIS_BOARD_CELLS;
    s.piece={r[m],r[m+1],int8_t(r[m+2]),int8_t(r[m+3])};s.next=r[m+4];s.state=TetrisState(r[m+5]);
    s.score=get(r+m+8);s.lines=get(r+m+12);s.level=get(r+m+16);s.random=get(r+m+20);
    s.fall_ms=get(r+m+24);s.clear_ms=get(r+m+28);s.clear_mask=get(r+m+32);
    if(!validate_tetris(s)) return false;
    out=s;return true;
}
TetrisPreferences encode_tetris_preferences(const TetrisSnapshot& s) {
    TetrisPreferences r{};r[0]='T';r[1]='P';r[2]=1;r[3]=s.volume;r[4]=s.music;r[5]=s.effects;
    put(r.data()+8,s.record);put(r.data()+16,checksum(r.data(),16));return r;
}
bool decode_tetris_preferences(const TetrisPreferences& r,TetrisSnapshot& s) {
    if(r[0]!='T'||r[1]!='P'||r[2]!=1||r[3]>100||r[4]>1||r[5]>1||get(r.data()+16)!=checksum(r.data(),16)) return false;
    s.record=get(r.data()+8);s.volume=r[3];s.music=r[4];s.effects=r[5];return true;
}
void TetrisGesture::press(int x,int y,int cell_width) {
    start_x_=anchor_x_=x;start_y_=y;cell_=std::max(1,cell_width);held_=true;drag_=soft_=false;
}
TetrisGestureAction TetrisGesture::move(int x,int y) {
    TetrisGestureAction a;if(!held_) return a;
    if(std::abs(x-start_x_)>=12||std::abs(y-start_y_)>=12) drag_=true;
    if(!drag_) return a;
    a.columns=std::clamp((x-anchor_x_)/cell_,-TETRIS_COLUMNS,TETRIS_COLUMNS);anchor_x_+=a.columns*cell_;
    const bool soft=y-start_y_>=12;
    if(soft!=soft_) {a.soft_changed=true;a.soft=soft;soft_=soft;}
    return a;
}
TetrisGestureAction TetrisGesture::release() {
    TetrisGestureAction a;if(held_) {a.rotate=!drag_;a.soft_changed=soft_;a.soft=false;}
    cancel();return a;
}
} // namespace el_sistema
