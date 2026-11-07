#pragma once
#include "Types.h"
#include <cstddef>
namespace voxel {
class Input {
public:
    InputState update(const TouchPoint* points,std::size_t count,bool paused);
    void reset();
    void cancelHeld();
private:
    enum class Role { Ignore,Move,Look,Jump,Mine,Place,Slot,Pause,Diagnostic };
    struct Contact { bool active=false; int id=0,x=0,y=0; Role role=Role::Ignore; };
    Contact contacts_[5]{};
    static Role roleAt(int x,int y,bool paused);
};
}
