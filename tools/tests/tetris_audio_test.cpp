#include "audio_session.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdio>
using namespace el_sistema;
int main() {
    for(int cycle=0;cycle<30;++cycle) {
        AudioSessionManager manager;
        assert(manager.try_acquire(AudioSessionOwner::Tetris));
        assert(!manager.try_acquire(AudioSessionOwner::Radar));
        std::atomic<bool> acquired{false},finished{false},release{false};
        std::thread priority([&] {
            AudioSessionLease lease(manager,AudioSessionOwner::Reminder);
            acquired.store(lease.acquired());
            while(!release.load()) std::this_thread::yield();
            finished.store(true);
        });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!manager.tetris_cancel_requested().load()) {
            assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();
        }
        manager.release(AudioSessionOwner::Tetris);
        // A continuously producing game cannot steal the codec while a voice
        // worker is waiting for it, nor during actual voice playback.
        while(!acquired.load()) {
            assert(!manager.try_acquire(AudioSessionOwner::Tetris));
            assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();
        }
        assert(manager.owner()==AudioSessionOwner::Reminder);
        assert(!manager.try_acquire(AudioSessionOwner::Tetris));
        release.store(true);priority.join();assert(finished.load());assert(!manager.busy());
    }
    std::puts("Audio: exclusión, cancelación de Tetris y prioridad de voz en 30 ciclos OK");
}
