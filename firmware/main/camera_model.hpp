#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {

inline constexpr unsigned CAMERA_WIDTH = 1280, CAMERA_HEIGHT = 720;
inline constexpr size_t CAMERA_RGB_BYTES = CAMERA_WIDTH * CAMERA_HEIGHT * 2;
inline constexpr size_t CAMERA_JPEG_LIMIT = 1024 * 1024;
inline constexpr size_t CAMERA_MEMORY_LIMIT = 8 * 1024 * 1024;
inline constexpr int64_t CAMERA_STALE_US = 1000000;
static_assert(2 * CAMERA_RGB_BYTES + 3 * CAMERA_JPEG_LIMIT + 65536 < CAMERA_MEMORY_LIMIT);

enum class CameraPhase : uint8_t { Off, Waiting, Preparing, Live, NoSignal, Error };
const char* camera_phase_text(CameraPhase phase);
bool camera_frame_fresh(int64_t timestamp_us, int64_t now_us);

// USB intervals are in units of 100 ns. Do not substitute a lower resolution.
struct CameraIntervals {
    std::array<uint32_t, 16> discrete{};
    size_t count = 0;
    uint32_t minimum = 0, maximum = 0, step = 0;
};
float camera_choose_fps(const CameraIntervals& intervals);

// Validate markers/lengths before passing untrusted USB bytes to the IDF JPEG
// decoder. Absent DHT is permitted: IDF supplies the standard MJPEG tables.
bool camera_valid_jpeg(const uint8_t* data, size_t size);

// Called under the service mutex. A displayed slot can NEVER be a decode target.
// The UI releases its old slot only after rebinding/dropping the LVGL image cache.
class CameraFrameSlots {
public:
    int begin_write();
    void publish(int slot, uint32_t session, uint32_t sequence, int64_t timestamp);
    void abort_write(int slot);
    int acquire_latest(uint32_t session);
    void release(int slot);
    bool idle() const;
    void reset();
    uint32_t sequence(int slot) const;
    int64_t timestamp(int slot) const;
    uint32_t discarded() const { return discarded_; }
private:
    enum class Use { Free, Writing, Ready, Displayed };
    struct Slot { Use use = Use::Free; uint32_t session = 0, sequence = 0; int64_t timestamp = 0; };
    std::array<Slot, 2> slots_{};
    uint32_t discarded_ = 0;
};

struct CameraSnapshot {
    bool available = false;
    CameraPhase phase = CameraPhase::Off;
    uint32_t session = 0, received = 0, decoded = 0, dropped = 0, invalid = 0;
    float source_fps = 0, decoded_fps = 0;
    uint32_t decode_us = 0;
    int64_t last_frame_us = 0;
    size_t video_bytes = 0;
    char error[192]{};
    char formats[1024]{};
};

struct CameraFrame {
    int slot = -1;
    const uint8_t* pixels = nullptr;
    uint32_t sequence = 0;
    int64_t timestamp_us = 0;
};

} // namespace el_sistema
