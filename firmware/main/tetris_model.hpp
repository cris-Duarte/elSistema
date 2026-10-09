#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {
constexpr int TETRIS_COLUMNS = 12, TETRIS_ROWS = 22, TETRIS_HIDDEN = 2;
constexpr int TETRIS_VISIBLE_ROWS = TETRIS_ROWS - TETRIS_HIDDEN;
constexpr int TETRIS_BOARD_CELLS = TETRIS_COLUMNS * TETRIS_ROWS;
constexpr int TETRIS_VISIBLE_CELLS = TETRIS_COLUMNS * TETRIS_VISIBLE_ROWS;
enum class TetrisState : uint8_t { Ready, Running, Paused, Finished };
enum TetrisSound : uint32_t { TetrisMove=1, TetrisRotate=2, TetrisLock=4, TetrisLines=8,
                             TetrisLevel=16, TetrisEnd=32 };
struct TetrisCell { int x, y; };
struct TetrisPiece { uint8_t kind=0, rotation=0; int8_t x=(TETRIS_COLUMNS-4)/2, y=0; };
struct TetrisSnapshot {
    std::array<uint8_t, TETRIS_COLUMNS*TETRIS_ROWS> board{};
    // Existing 10-column sessions keep their geometry until a new game.
    uint8_t columns=TETRIS_COLUMNS;
    TetrisPiece piece{};
    uint8_t next=0;
    TetrisState state=TetrisState::Ready;
    uint32_t score=0, record=0, lines=0, level=1, random=1;
    uint32_t fall_ms=0, clear_ms=0, clear_mask=0, generation=0;
    bool music=true, effects=true;
    uint8_t volume=35;
    bool storage_error=false, corrupt=false, available=true;
};
std::array<TetrisCell,4> tetris_cells(const TetrisPiece& piece);
uint32_t tetris_fall_interval(uint32_t level);
uint32_t tetris_line_score(unsigned count, uint32_t level);
bool validate_tetris(const TetrisSnapshot& state);
class TetrisEngine {
public:
    const TetrisSnapshot& snapshot() const { return state_; }
    void start(uint32_t seed);
    bool restore(const TetrisSnapshot& state);
    void pause();
    void resume();
    bool move(int columns);
    bool rotate();
    void set_soft_drop(bool enabled);
    void advance(uint32_t milliseconds);
    void settings(bool music, bool effects, uint8_t volume);
    uint32_t take_sounds() { const auto result=sounds_; sounds_=0; return result; }
private:
    bool fits(const TetrisPiece& piece) const;
    bool descend();
    void spawn();
    void lock();
    void finish_clear();
    uint8_t random_piece();
    void changed() { ++state_.generation; }
    TetrisSnapshot state_{};
    bool soft_=false;
    uint32_t sounds_=0;
};
// Portable, explicit little-endian records. Preferences/high score are stored
// separately so a damaged session cannot discard a valid personal record.
using TetrisRecord=std::array<uint8_t,8+TETRIS_BOARD_CELLS+56>;
TetrisRecord encode_tetris(const TetrisSnapshot& state);
bool decode_tetris(const TetrisRecord& record, TetrisSnapshot& state);
bool decode_tetris(const uint8_t* record, size_t size, TetrisSnapshot& state);
using TetrisPreferences=std::array<uint8_t,20>;
TetrisPreferences encode_tetris_preferences(const TetrisSnapshot& state);
bool decode_tetris_preferences(const TetrisPreferences& record, TetrisSnapshot& state);
struct TetrisGestureAction { int columns=0; bool rotate=false, soft_changed=false, soft=false; };
class TetrisGesture {
public:
    void press(int x,int y,int cell_width);
    TetrisGestureAction move(int x,int y);
    TetrisGestureAction release();
    void cancel() { held_=drag_=soft_=false; }
private:
    int start_x_=0,start_y_=0,anchor_x_=0,cell_=1;
    bool held_=false,drag_=false,soft_=false;
};
} // namespace el_sistema
