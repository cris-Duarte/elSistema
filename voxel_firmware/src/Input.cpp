#include "Input.h"
#include <algorithm>
#include <cmath>
namespace voxel {
void Input::reset() { for(auto& c:contacts_) c={}; }
void Input::cancelHeld() { for(auto& c:contacts_) if(c.active) c.role=Role::Ignore; }
Input::Role Input::roleAt(int x,int y,bool paused) {
    if(x>=1140 && y<100) return Role::Pause;
    if(x<140 && y<100) return Role::Diagnostic;
    if(paused) return x>=440 && x<840 && y>=280 && y<410 ? Role::Pause : Role::Ignore;
    if(y>=610 && x>=400 && x<880) return Role::Slot;
    if(x<370 && y>=380) return Role::Move;
    if(x>=930 && x<1060 && y>=560) return Role::Mine;
    if(x>=1080 && y>=560) return Role::Place;
    if(x>=1080 && y>=400 && y<540) return Role::Jump;
    if(x>=500) return Role::Look;
    return Role::Ignore;
}
InputState Input::update(const TouchPoint* points,std::size_t count,bool paused) {
    InputState out;
    count=std::min<std::size_t>(count,5);
    // Retain ownership by ID even if the controller changes point ordering.
    for(auto& c:contacts_) if(c.active) {
        bool found=false;
        for(std::size_t i=0;i<count;++i) if(points[i].id==c.id) found=true;
        if(!found) c={};
    }
    bool moveOwned=false,lookOwned=false;
    for(const auto& c:contacts_) if(c.active) { moveOwned|=c.role==Role::Move; lookOwned|=c.role==Role::Look; }
    for(std::size_t i=0;i<count;++i) {
        const auto& p=points[i];
        if(p.x<0 || p.x>=1280 || p.y<0 || p.y>=720) continue;
        Contact* contact=nullptr;
        bool fresh=false;
        for(auto& c:contacts_) if(c.active && c.id==p.id) { contact=&c; break; }
        if(!contact) for(auto& c:contacts_) if(!c.active) {
            c={true,p.id,p.x,p.y,roleAt(p.x,p.y,paused)};
            if(c.role==Role::Move) { if(moveOwned) c.role=Role::Ignore; else moveOwned=true; }
            if(c.role==Role::Look) { if(lookOwned) c.role=Role::Ignore; else lookOwned=true; }
            contact=&c; fresh=true; break;
        }
        if(!contact) continue;
        switch(contact->role) {
        case Role::Move: {
            const float dx=(p.x-180)/125.f,dy=(550-p.y)/125.f;
            const float radius=std::sqrt(dx*dx+dy*dy);
            if(radius>.15f) {
                const float magnitude=std::min(1.f,(radius-.15f)/.85f);
                out.strafe=dx/radius*magnitude; out.forward=dy/radius*magnitude;
            }
            break;
        }
        case Role::Look:
            if(!fresh) { out.lookX=(p.x-contact->x)*.003f; out.lookY=(p.y-contact->y)*-.003f; }
            break;
        case Role::Jump: out.jump|=fresh; break;
        case Role::Mine: out.mine|=fresh; break;
        case Role::Place: out.place|=fresh; break;
        case Role::Slot: if(fresh) out.slot=std::clamp((p.x-400)/80,0,5); break;
        case Role::Pause: out.pause|=fresh; break;
        case Role::Diagnostic: out.diagnostic|=fresh; break;
        default: break;
        }
        contact->x=p.x; contact->y=p.y;
    }
    return out;
}
}
