#include "tetris_model.hpp"
#include "tetris_sound.hpp"
#include "tetris_layout.hpp"
#include "modules.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
using namespace el_sistema;

static void pieces_and_collisions(int columns) {
    for(uint8_t kind=0;kind<7;++kind) for(uint8_t rotation=0;rotation<4;++rotation) {
        auto cells=tetris_cells({kind,rotation,3,0});std::set<int> unique;
        for(auto c:cells) {assert(c.x>=3&&c.x<7&&c.y>=0&&c.y<4);unique.insert(c.y*10+c.x);}
        assert(unique.size()==4);
        TetrisSnapshot s;s.columns=columns;s.state=TetrisState::Paused;s.piece={kind,rotation,3,6};
        TetrisEngine game;assert(game.restore(s));game.resume();
        const auto before=tetris_cells(game.snapshot().piece);
        assert(game.rotate());
        if(kind==1) {
            const auto after=tetris_cells(game.snapshot().piece);
            for(int i=0;i<4;++i) assert(before[i].x==after[i].x&&before[i].y==after[i].y);
        }
        assert(game.move(-columns));assert(!game.move(-1));
        assert(game.move(columns));assert(!game.move(1));
        for(auto c:tetris_cells(game.snapshot().piece)) assert(c.x>=0&&c.x<columns);
    }
    TetrisEngine game;TetrisSnapshot s;s.state=TetrisState::Paused;s.piece={0,1,-2,6};
    assert(game.restore(s));game.resume();assert(!game.rotate()); // no wall kick
    const auto x=game.snapshot().piece.x;assert(!game.move(-1));assert(game.snapshot().piece.x==x);
}
static void clearing_and_score(int columns) {
    for(unsigned n=1;n<=4;++n) {
        TetrisSnapshot s;s.columns=columns;s.state=TetrisState::Paused;s.piece={0,1,1,18};s.lines=9;
        for(unsigned row=22-n;row<22;++row) for(int x=0;x<columns;++x) if(x!=3) s.board[row*columns+x]=2;
        TetrisEngine game;assert(game.restore(s));game.resume();game.advance(800);
        assert(game.snapshot().clear_ms==200);assert(game.snapshot().state==TetrisState::Running);
        const auto p=game.snapshot().piece;assert(!game.move(1)&&!game.rotate());
        game.set_soft_drop(true);game.advance(100);assert(game.snapshot().piece.x==p.x);
        // A checkpoint during a clearing flash resumes the pending transition
        // once, neither losing nor awarding the same lines twice.
        const auto raw=encode_tetris(game.snapshot());TetrisSnapshot restored;
        assert(decode_tetris(raw,restored));TetrisEngine copy;assert(copy.restore(restored));
        copy.advance(1000);assert(copy.snapshot().score==0);copy.resume();copy.advance(100);
        assert(copy.snapshot().score==tetris_line_score(n,1));assert(copy.snapshot().lines==9+n);
        assert(copy.snapshot().level==2);assert(copy.snapshot().record==copy.snapshot().score);
        assert(copy.snapshot().clear_ms==0);assert(copy.snapshot().board[TETRIS_ROWS*columns-1]==0);
    }
    assert(tetris_line_score(1,1)==40);assert(tetris_line_score(2,3)==300);
    assert(tetris_line_score(3,2)==600);assert(tetris_line_score(4,1)==1200);
    assert(tetris_fall_interval(1)==800);assert(tetris_fall_interval(2)==680);assert(tetris_fall_interval(100)==80);
}
static void lifecycle_and_serialization() {
    TetrisEngine switching;switching.start(8);switching.advance(790);
    const auto initial_y=switching.snapshot().piece.y;
    switching.set_soft_drop(true);switching.advance(39);
    assert(switching.snapshot().piece.y==initial_y&&switching.snapshot().score==0);
    switching.advance(1);assert(switching.snapshot().piece.y==initial_y+1&&switching.snapshot().score==1);
    switching.set_soft_drop(false);switching.advance(799);assert(switching.snapshot().piece.y==initial_y+1);
    switching.advance(1);assert(switching.snapshot().piece.y==initial_y+2);
    TetrisEngine game;game.start(42);const auto y=game.snapshot().piece.y;
    game.advance(799);assert(game.snapshot().piece.y==y);game.advance(1);assert(game.snapshot().piece.y==y+1);
    game.set_soft_drop(true);game.advance(40);assert(game.snapshot().score==1);
    game.pause();auto paused=game.snapshot();game.advance(1000);
    assert(game.snapshot().piece.y==paused.piece.y);assert(!game.move(1)&&!game.rotate());
    game.resume();game.advance(40);assert(game.snapshot().score==1); // soft drop released
    auto raw=encode_tetris(game.snapshot());TetrisSnapshot restored;assert(decode_tetris(raw,restored));
    TetrisEngine copy;assert(copy.restore(restored));assert(copy.snapshot().state==TetrisState::Paused);
    assert(copy.snapshot().fall_ms==game.snapshot().fall_ms);
    assert(copy.snapshot().next==game.snapshot().next);assert(copy.snapshot().random==game.snapshot().random);
    raw[10]^=1;assert(!decode_tetris(raw,restored));
    restored=game.snapshot();restored.board[80]=8;assert(!copy.restore(restored));
    restored=game.snapshot();restored.next=7;assert(!copy.restore(restored));
    restored=game.snapshot();restored.level=100;assert(!copy.restore(restored));
    game.settings(false,true,70);auto prefs=encode_tetris_preferences(game.snapshot());
    TetrisSnapshot pref;assert(decode_tetris_preferences(prefs,pref));
    assert(!pref.music&&pref.effects&&pref.volume==70&&pref.record==1);
    prefs[8]^=1;assert(!decode_tetris_preferences(prefs,pref));
    game.start(123);assert(game.snapshot().score==0&&game.snapshot().record==1);
    assert(!game.snapshot().music&&game.snapshot().volume==70);
    // A normal lock in the hidden rows ends the game.
    TetrisSnapshot top;top.state=TetrisState::Paused;top.piece={1,0,3,0};
    top.board[2*TETRIS_COLUMNS+4]=1;top.board[2*TETRIS_COLUMNS+5]=1;
    assert(copy.restore(top));copy.resume();copy.advance(800);assert(copy.snapshot().state==TetrisState::Finished);
    assert(copy.take_sounds()&TetrisEnd);
}
static void gestures() {
    TetrisGesture g;g.press(100,100,50);auto a=g.move(105,105);assert(!a.columns&&!a.soft_changed);
    assert(g.release().rotate);assert(!g.release().rotate);
    g.press(100,100,50);a=g.move(112,100);assert(!a.columns);assert(!g.release().rotate);
    g.press(100,100,50);assert(g.move(201,100).columns==2);assert(g.move(202,100).columns==0);
    assert(g.move(151,100).columns==0);assert(g.move(149,100).columns==-1);assert(!g.release().rotate);
    g.press(100,100,50);a=g.move(100,112);assert(a.soft_changed&&a.soft);
    a=g.move(100,170);assert(!a.soft_changed);a=g.release();assert(a.soft_changed&&!a.soft&&!a.rotate);
    g.press(100,100,50);g.move(100,150);g.cancel();a=g.release();assert(!a.rotate&&!a.soft);
    // Full-width columns use the same pitch for drawing and gestures.
    g.press(100,200,tetris_layout::cell_width);
    assert(g.move(159,200).columns==0);assert(g.move(160,200).columns==1);
    assert(g.move(220,200).columns==1);assert(!g.release().rotate);
    for(uint8_t kind=0;kind<7;++kind) for(auto c:tetris_cells({kind,0,0,0})) {
        const int x=tetris_layout::preview_x+c.x*tetris_layout::preview_cell;
        const int y=tetris_layout::preview_y+c.y*tetris_layout::preview_cell;
        assert(x>=0&&x+tetris_layout::preview_cell<=tetris_layout::screen_width);
        assert(y>=0&&y+tetris_layout::preview_cell<=tetris_layout::header_height);
    }
}
static uint32_t record_crc(const uint8_t* data,size_t length) {
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<length;++i) {crc^=data[i];for(int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320:0);}
    return ~crc;
}
static void write_u32(uint8_t* data,uint32_t value) {
    for(int i=0;i<4;++i) data[i]=uint8_t(value>>(i*8));
}
static std::array<uint8_t,280> legacy_record(const TetrisSnapshot& s) {
    std::array<uint8_t,280> r{};r[0]='T';r[1]='E';r[2]='T';r[3]=1;
    std::copy(s.board.begin(),s.board.begin()+220,r.begin()+4);
    r[224]=s.piece.kind;r[225]=s.piece.rotation;r[226]=uint8_t(s.piece.x);r[227]=uint8_t(s.piece.y);
    r[228]=s.next;r[229]=uint8_t(s.state);
    write_u32(r.data()+232,s.score);write_u32(r.data()+236,s.lines);write_u32(r.data()+240,s.level);
    write_u32(r.data()+244,s.random);write_u32(r.data()+248,s.fall_ms);write_u32(r.data()+252,s.clear_ms);
    write_u32(r.data()+256,s.clear_mask);write_u32(r.data()+276,record_crc(r.data(),276));return r;
}
static void legacy_sessions() {
    for(bool clearing:{false,true}) {
        TetrisSnapshot old;old.columns=10;old.state=TetrisState::Running;old.piece={0,1,1,18};old.score=1234;
        old.random=42;old.next=6;old.fall_ms=299;old.board[205]=3;
        if(clearing) {
            old.clear_ms=100;old.clear_mask=1u<<21;
            for(int x=0;x<10;++x) old.board[210+x]=2;
        }
        const auto raw=legacy_record(old);TetrisSnapshot recovered;
        assert(decode_tetris(raw.data(),raw.size(),recovered));assert(recovered.columns==10);
        assert(recovered.board==old.board&&recovered.clear_ms==old.clear_ms);
        assert(recovered.score==old.score&&recovered.fall_ms==old.fall_ms&&recovered.piece.x==old.piece.x);
        TetrisEngine actual,expected;assert(actual.restore(recovered)&&expected.restore(old));
        assert(actual.snapshot().state==TetrisState::Paused);actual.resume();expected.resume();
        actual.advance(100);expected.advance(100);
        assert(actual.snapshot().board==expected.snapshot().board&&actual.snapshot().score==expected.snapshot().score);
        assert(actual.snapshot().next==expected.snapshot().next&&actual.snapshot().columns==10);
        // A subsequent checkpoint upgrades the envelope without widening the game.
        const auto upgraded=encode_tetris(actual.snapshot());assert(upgraded[3]==2);
        assert(decode_tetris(upgraded,recovered)&&recovered.columns==10);
        assert(recovered.board==actual.snapshot().board);
        actual.start(77);assert(actual.snapshot().columns==12&&actual.snapshot().piece.x==4);
        assert(actual.snapshot().record==expected.snapshot().record);
        assert(!decode_tetris(raw.data(),279,recovered));
        auto damaged=raw;damaged[40]^=1;assert(!decode_tetris(damaged.data(),damaged.size(),recovered));
    }
    TetrisSnapshot state;auto raw=encode_tetris(state);raw[4]=11;
    write_u32(raw.data()+raw.size()-4,record_crc(raw.data(),raw.size()-4));
    assert(!decode_tetris(raw,state));assert(!decode_tetris(nullptr,0,state));
    state={};state.columns=10;state.board[220]=1;assert(!validate_tetris(state));
    state={};state.columns=12;state.state=TetrisState::Running;state.piece={1,0,8,4};
    TetrisEngine wide;assert(wide.restore(state));wide.resume();assert(wide.move(1));
    for(auto c:tetris_cells(wide.snapshot().piece)) assert(c.x>=10); // new columns are playable
}
static void random_and_audio() {
    std::set<unsigned> kinds;TetrisEngine game;
    for(uint32_t seed=1;seed<=100;++seed) {game.start(seed);kinds.insert(game.snapshot().piece.kind);assert(validate_tetris(game.snapshot()));}
    assert(kinds.size()==7);TetrisEngine a,b;a.start(42);b.start(42);
    assert(a.snapshot().piece.kind==b.snapshot().piece.kind&&a.snapshot().next==b.snapshot().next);
    TetrisSynth synth;int16_t pcm[320];synth.render(pcm,320,true,0);
    for(auto value:pcm) assert(value==0);
    synth.reset();synth.render(pcm,320,true,35);assert(std::any_of(std::begin(pcm),std::end(pcm),[](int16_t v){return v!=0;}));
    synth.reset();synth.render(pcm,320,false,35);for(auto value:pcm) assert(value==0);
    for(auto sound:{TetrisMove,TetrisRotate,TetrisLock,TetrisLines,TetrisLevel,TetrisEnd}) {
        synth.reset();synth.trigger(sound);synth.render(pcm,320,false,100);
        assert(std::any_of(std::begin(pcm),std::end(pcm),[](int16_t v){return v!=0;}));
        for(auto v:pcm) assert(v<32000&&v>-32000);
    }
    for (int removed : {2,3,5}) assert(!find_module(static_cast<ModuleId>(removed)));
    assert(kModules.size()==4&&find_module(ModuleId::Productivity)&&find_module(ModuleId::Assistant)&&find_module(ModuleId::Tetris)&&find_module(ModuleId::Habits));
}
int main() {
    for(int columns:{10,12}) {pieces_and_collisions(columns);clearing_and_score(columns);}
    lifecycle_and_serialization();legacy_sessions();gestures();random_and_audio();
    TetrisEngine stress;stress.start(7);
    for(unsigned frame=0;frame<100000;++frame) {
        if(stress.snapshot().state==TetrisState::Finished) stress.start(frame+1);
        if(frame%3==0) stress.move(frame%2?1:-1);
        if(frame%7==0) stress.rotate();
        stress.set_soft_drop(frame%4==0);stress.advance(20);
        assert(validate_tetris(stress.snapshot()));
        if(frame%53==0) {TetrisSnapshot decoded;assert(decode_tetris(encode_tetris(stress.snapshot()),decoded));}
    }
    std::puts("Tetris: 12 columnas, compatibilidad de sesiones clásicas, cuadrados, piezas, filas, CRC, gestos y síntesis OK");
}
