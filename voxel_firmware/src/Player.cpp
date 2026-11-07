#include "Player.h"
#include <algorithm>
namespace voxel {
namespace {
constexpr float Epsilon=.0001f;
bool blocked(const World& w,int x,int y,int z) { return !World::contains(x,y,z) || solid(w.get(x,y,z)); }
void bounds(Vec3 p,int lo[3],int hi[3]) {
    lo[0]=static_cast<int>(std::floor(p.x-cfg::HalfWidth+Epsilon));
    lo[1]=static_cast<int>(std::floor(p.y-cfg::HalfWidth+Epsilon));
    lo[2]=static_cast<int>(std::floor(p.z+Epsilon));
    hi[0]=static_cast<int>(std::floor(p.x+cfg::HalfWidth-Epsilon));
    hi[1]=static_cast<int>(std::floor(p.y+cfg::HalfWidth-Epsilon));
    hi[2]=static_cast<int>(std::floor(p.z+cfg::PlayerHeight-Epsilon));
}
}
bool Player::collides(const World& w,Vec3 p) const {
    int lo[3],hi[3]; bounds(p,lo,hi);
    for(int z=lo[2];z<=hi[2];++z) for(int y=lo[1];y<=hi[1];++y) for(int x=lo[0];x<=hi[0];++x)
        if(blocked(w,x,y,z)) return true;
    return false;
}
bool Player::overlaps(Cell c) const {
    return feet.x+cfg::HalfWidth>c.x+Epsilon && feet.x-cfg::HalfWidth<c.x+1-Epsilon &&
           feet.y+cfg::HalfWidth>c.y+Epsilon && feet.y-cfg::HalfWidth<c.y+1-Epsilon &&
           feet.z+cfg::PlayerHeight>c.z+Epsilon && feet.z<c.z+1-Epsilon;
}
bool Player::spawn(const World& w) {
    for(int radius=0;radius<30;++radius) for(int y=32-radius;y<=32+radius;++y) for(int x=32-radius;x<=32+radius;++x) {
        if(radius && std::abs(x-32)!=radius && std::abs(y-32)!=radius) continue;
        for(int z=cfg::WorldZ-3;z>=0;--z) if(solid(w.get(x,y,z))) {
            const Vec3 p={x+.5f,y+.5f,z+1.f};
            if(collides(w,p)) continue;
            feet=p; verticalSpeed=0; grounded=true;
            camera.yaw=.65f; camera.pitch=-.12f; syncCamera(); return true;
        }
    }
    return false;
}
void Player::syncCamera() { camera.position=feet+Vec3{0,0,cfg::EyeHeight}; camera.updateBasis(); }
void Player::moveAxis(const World& w,int axis,float amount) {
    if(amount==0) return;
    Vec3 p=feet;
    float values[]={p.x,p.y,p.z}; values[axis]+=amount;
    p={values[0],values[1],values[2]};
    int lo[3],hi[3]; bounds(p,lo,hi);
    bool contact=false;
    float resolved=values[axis];
    const float lower=axis==2?0.f:-cfg::HalfWidth;
    const float upper=axis==2?cfg::PlayerHeight:cfg::HalfWidth;
    for(int z=lo[2];z<=hi[2];++z) for(int y=lo[1];y<=hi[1];++y) for(int x=lo[0];x<=hi[0];++x) {
        if(!blocked(w,x,y,z)) continue;
        const int cell[]={x,y,z};
        resolved=amount>0?std::min(resolved,cell[axis]-upper):std::max(resolved,cell[axis]+1.f-lower);
        contact=true;
    }
    values[axis]=resolved; feet={values[0],values[1],values[2]};
    if(axis==2 && contact) { verticalSpeed=0; if(amount<0) grounded=true; }
}
void Player::step(const World& w,const InputState& in,float dt) {
    if(dt<=0 || dt>.1f) return;
    float sx=std::clamp(in.strafe,-1.f,1.f),fw=std::clamp(in.forward,-1.f,1.f);
    const float n=std::sqrt(sx*sx+fw*fw);
    if(n>1) { sx/=n; fw/=n; }
    if(in.jump && grounded) { verticalSpeed=cfg::JumpSpeed; grounded=false; }
    verticalSpeed=std::max(-20.f,verticalSpeed+cfg::Gravity*dt);
    const float cy=std::cos(camera.yaw),sy=std::sin(camera.yaw);
    const Vec3 delta={(cy*fw+sy*sx)*cfg::Speed*dt,(sy*fw-cy*sx)*cfg::Speed*dt,verticalSpeed*dt};
    const int parts=std::max(1,static_cast<int>(std::ceil(std::max({std::abs(delta.x),std::abs(delta.y),std::abs(delta.z)})/.25f)));
    grounded=false;
    for(int i=0;i<parts;++i) {
        moveAxis(w,0,delta.x/parts); moveAxis(w,1,delta.y/parts); moveAxis(w,2,delta.z/parts);
    }
    syncCamera();
}
}
