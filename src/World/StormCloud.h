/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// A fixed, irregular collection of soft smoke lobes. Kinect-coordinate
// movement is elapsed-time based and independent of application frame rate.
class StormCloud {
public:
    struct Lobe {float x,y,rx,ry,angle;};
    // PUFFY_CLOUDS_V1: rounded crown, broad underside, soft overlapping puffs.
    // The same lobes drive visible cloud geometry and the rain footprint.
    const std::array<Lobe,7> lobes{{
        {0,.10f,.73f,.40f,0},
        {-.43f,-.13f,.40f,.39f,0},
        {0,-.27f,.46f,.48f,0},
        {.47f,-.07f,.38f,.36f,0},
        {-.46f,.24f,.39f,.30f,0},
        {.43f,.25f,.40f,.30f,0},
        {0,.32f,.59f,.25f,0}
    }};
    float originX=0,originY=0,x=0,y=0,radius=0,angle=0;
    float dx=0,dy=0,driftAt=-1;
    bool active=false;
    float wanderAt=-1,wanderX=0,wanderY=0,wanderVX=0,wanderVY=0;
    void reset(){active=false;driftAt=-1;radius=0;wanderAt=-1;}
    void form(float px,float py,float r,float rotation,float vx,float vy) {
        originX=x=px;originY=y=py;radius=r;angle=rotation;
        dx=vx;dy=vy;active=r>0;driftAt=-1;wanderAt=-1;
    }
    void start(float now){if(active&&driftAt<0)driftAt=now;}
    void advance(float now,float left,float top,float width,float height) {
        if(!active)return;
        float age=driftAt<0?0:std::max(0.0f,now-driftAt);
        x=originX+dx*age;y=originY+dy*age;
        // 1.2R conservatively encloses every rotated smoke lobe.
        float extent=radius*1.2f;
        if(x+extent<left||x-extent>left+width||y+extent<top||y-extent>top+height)active=false;
    }
    void wander(float now,float left,float top,float width,float height) {
        if(!active)return;
        if(wanderAt<0) {
            wanderAt=driftAt>=0?driftAt:now;wanderX=x;wanderY=y;wanderVX=dx;wanderVY=dy;
        }
        float elapsed=std::max(0.0f,now-wanderAt);
        auto reflect=[&](float origin,float velocity,float& position,float& currentVelocity,float lo,float hi) {
            float span=hi-lo;
            if(span<=0){position=(lo+hi)*.5f;return;}
            float phase=std::fmod(origin+velocity*elapsed-lo,2*span);
            if(phase<0)phase+=2*span;
            bool reverse=phase>span;
            position=lo+(reverse?2*span-phase:phase);
            currentVelocity=reverse?-velocity:velocity;
        };
        float margin=radius*1.2f;
        reflect(wanderX,wanderVX,x,dx,left+margin,left+width-margin);
        reflect(wanderY,wanderVY,y,dy,top+margin,top+height-margin);
        originX=x;originY=y;driftAt=now;
    }
    void lobeCenter(const Lobe& l,float& px,float& py) const {
        float ca=std::cos(angle),sa=std::sin(angle);
        px=x+radius*(l.x*ca-l.y*sa);py=y+radius*(l.x*sa+l.y*ca);
    }
    bool covers(float px,float py) const {
        if(!active||radius<=0)return false;
        if(std::abs(px-x)>radius*1.2f||std::abs(py-y)>radius*1.2f)return false;
        for(const auto& l:lobes) {
            float cx,cy;lobeCenter(l,cx,cy);
            float ca=std::cos(angle+l.angle),sa=std::sin(angle+l.angle);
            float u=((px-cx)*ca+(py-cy)*sa)/(radius*l.rx);
            float v=(-(px-cx)*sa+(py-cy)*ca)/(radius*l.ry);
            if(u*u+v*v<=1)return true;
        }
        return false;
    }
};
