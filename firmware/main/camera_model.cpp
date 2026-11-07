#include "camera_model.hpp"
#include <cmath>

namespace el_sistema {
const char* camera_phase_text(CameraPhase p)
{
    switch (p) {
        case CameraPhase::Off: return "Monitor detenido";
        case CameraPhase::Waiting: return "Conecta la cámara";
        case CameraPhase::Preparing: return "Preparando";
        case CameraPhase::Live: return "En vivo";
        case CameraPhase::NoSignal: return "Sin señal";
        case CameraPhase::Error: return "Error";
    }
    return "Error";
}
bool camera_frame_fresh(int64_t stamp, int64_t now)
{
    return stamp > 0 && now >= stamp && now - stamp < CAMERA_STALE_US;
}
float camera_choose_fps(const CameraIntervals& f)
{
    for (const float requested : {30.f, 25.f}) {
        const uint32_t ideal = static_cast<uint32_t>(std::lround(10000000.0 / requested));
        for (size_t n = 0; n < f.count && n < f.discrete.size(); ++n) {
            const uint32_t interval = f.discrete[n];
            if (interval && std::abs(10000000.f / interval - requested) < .15f)
                return 10000000.f / interval;
        }
        if (!f.count && f.minimum && f.minimum <= ideal && f.maximum >= ideal) {
            uint32_t interval = ideal;
            if (f.step) interval = f.minimum + static_cast<uint32_t>(
                std::llround(double(ideal - f.minimum) / f.step)) * f.step;
            if (interval <= f.maximum && std::abs(10000000.f / interval - requested) < .15f)
                return 10000000.f / interval;
        }
    }
    return 0;
}
bool camera_valid_jpeg(const uint8_t* d, size_t size)
{
    if (!d || size < 16 || size > CAMERA_JPEG_LIMIT || d[0] != 0xff || d[1] != 0xd8 ||
        d[size-2] != 0xff || d[size-1] != 0xd9) return false;
    bool frame = false;
    uint8_t quant_tables = 0, required_quant = 0, dc_tables = 0, ac_tables = 0;
    bool has_huffman = false;
    std::array<uint8_t,3> component_ids{};
    size_t at = 2;
    while (at + 3 < size) {
        if (d[at++] != 0xff) return false;
        while (at < size && d[at] == 0xff) ++at;
        if (at >= size) return false;
        const uint8_t marker = d[at++];
        if (marker == 0 || marker == 0xd8 || marker == 0xd9 || marker == 0x01 ||
            (marker >= 0xd0 && marker <= 0xd7)) return false;
        if (marker != 0xc0 && marker != 0xc4 && marker != 0xdb && marker != 0xdd &&
            marker != 0xda && marker != 0xfe && !(marker >= 0xe0 && marker <= 0xef)) return false;
        if (at + 2 > size) return false;
        const size_t length = (size_t(d[at]) << 8) | d[at+1];
        if (length < 2 || length > size - at) return false;
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
            if (marker != 0xc0 || frame || length < 8 || d[at+2] != 8) return false;
            const unsigned height = (unsigned(d[at+3]) << 8) | d[at+4];
            const unsigned width = (unsigned(d[at+5]) << 8) | d[at+6];
            const unsigned components = d[at+7];
            if (width != CAMERA_WIDTH || height != CAMERA_HEIGHT || components != 3 ||
                length != 8 + 3 * components) return false;
            for (unsigned c = 0; c < 3; ++c) {
                const size_t base = at + 8 + 3 * c;
                component_ids[c] = d[base];
                for (unsigned previous = 0; previous < c; ++previous)
                    if (component_ids[previous] == component_ids[c]) return false;
                if (d[base+2] > 1) return false;
                required_quant |= 1 << d[base+2];
                const uint8_t sampling = d[base+1];
                if (c == 0 ? (sampling != 0x11 && sampling != 0x21 && sampling != 0x22) : sampling != 0x11)
                    return false;
            }
            frame = true;
        }
        if (marker == 0xdd && length != 4) return false;
        if (marker == 0xdb) {
            size_t pos = at + 2;
            while (pos < at + length) {
                // The P4 baseline decoder accepts 8-bit tables 0 and 1.
                const uint8_t table = d[pos++];
                if (table > 1 || at + length - pos < 64 || (quant_tables & (1 << table))) return false;
                for (size_t i = 0; i < 64; ++i) if (!d[pos+i]) return false;
                quant_tables |= 1 << table; pos += 64;
            }
        }
        if (marker == 0xc4) {
            has_huffman = true;
            size_t pos = at + 2;
            while (pos < at + length) {
                if (at + length - pos < 17) return false;
                const uint8_t table = d[pos++];
                if ((table != 0 && table != 1 && table != 0x10 && table != 0x11)) return false;
                size_t symbols = 0;
                for (unsigned i = 0; i < 16; ++i) symbols += d[pos++];
                if (!symbols || symbols > 256 || symbols > at + length - pos) return false;
                if (table & 0x10) ac_tables |= 1 << (table & 1);
                else dc_tables |= 1 << table;
                pos += symbols;
            }
        }
        if (marker == 0xda) {
            if (!frame || length != 12 || d[at+2] != 3 || at + length >= size - 2) return false;
            for (unsigned c = 0; c < 3; ++c) {
                if (d[at+3+2*c] != component_ids[c]) return false;
                const uint8_t tables = d[at+4+2*c];
                if ((tables >> 4) > 1 || (tables & 15) > 1) return false;
                if (has_huffman && (!(dc_tables & (1 << (tables >> 4))) || !(ac_tables & (1 << (tables & 15))))) return false;
            }
            // IDF 5.4.2 installs standard Huffman tables if DHT is absent (a
            // valid/common MJPEG optimization). Partially declared tables fail.
            return (quant_tables & required_quant) == required_quant && d[at+9] == 0 && d[at+10] == 63 && d[at+11] == 0;
        }
        at += length;
    }
    return false;
}
int CameraFrameSlots::begin_write()
{
    for (int n = 0; n < 2; ++n) if (slots_[n].use == Use::Free) {
        slots_[n].use = Use::Writing; return n;
    }
    for (int n = 0; n < 2; ++n) if (slots_[n].use == Use::Ready) {
        ++discarded_; slots_[n].use = Use::Writing; return n;
    }
    return -1;
}
void CameraFrameSlots::publish(int slot, uint32_t session, uint32_t seq, int64_t stamp)
{
    if (slot >= 0 && slot < 2 && slots_[slot].use == Use::Writing)
        slots_[slot] = {Use::Ready, session, seq, stamp};
}
void CameraFrameSlots::abort_write(int slot)
{
    if (slot >= 0 && slot < 2 && slots_[slot].use == Use::Writing) slots_[slot].use = Use::Free;
}
int CameraFrameSlots::acquire_latest(uint32_t session)
{
    int found = -1;
    for (int n = 0; n < 2; ++n) if (slots_[n].use == Use::Ready) {
        if (slots_[n].session != session) { ++discarded_; slots_[n].use = Use::Free; continue; }
        if (found < 0 || slots_[n].timestamp > slots_[found].timestamp) found = n;
    }
    if (found >= 0) {
        slots_[found].use = Use::Displayed;
        for (auto& slot : slots_) if (slot.use == Use::Ready) { ++discarded_; slot.use = Use::Free; }
    }
    return found;
}
void CameraFrameSlots::release(int slot)
{
    if (slot >= 0 && slot < 2 && slots_[slot].use == Use::Displayed) slots_[slot].use = Use::Free;
}
bool CameraFrameSlots::idle() const
{
    for (const auto& slot : slots_) if (slot.use == Use::Writing || slot.use == Use::Displayed) return false;
    return true;
}
void CameraFrameSlots::reset() { if (idle()) { slots_ = {}; discarded_ = 0; } }
uint32_t CameraFrameSlots::sequence(int slot) const { return slots_[slot].sequence; }
int64_t CameraFrameSlots::timestamp(int slot) const { return slots_[slot].timestamp; }
} // namespace el_sistema
