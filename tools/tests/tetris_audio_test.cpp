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
        assert(!manager.try_acquire(AudioSessionOwner::Tetris));
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
    for(auto priority:{AudioSessionOwner::DictationCapture,AudioSessionOwner::AssistantCapture,AudioSessionOwner::Reminder}) {
        AudioSessionManager manager;assert(manager.try_acquire(AudioSessionOwner::HabitNotice));
        std::atomic<bool> acquired{false};
        std::thread worker([&]{AudioSessionLease lease(manager,priority);acquired=lease.acquired();});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!manager.habit_notice_cancel_requested().load()) {assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
        manager.release(AudioSessionOwner::HabitNotice);worker.join();assert(acquired);assert(!manager.busy());
    }
    std::puts("Audio: los avisos de hábitos ceden ante captura, asistente y Productividad OK");
}
