#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <algorithm>
// WAVE_INPUT_V73: accumulate a sweep across samples instead of requiring a
// large displacement every frame. All distance/speed tests use millimetres.
// A held hand cannot repeatedly inject energy; missing depth gets a short grace.
class WaterSweep {
public:
    struct Push { bool fire=false; float x=0,y=0,dx=0,dy=0,radius=0,strength=0,speed=0; };
    void reset(){active=false;lastSeen=-1;next=0;}
    Push sample(float now,float x,float y,float area,float cellX,float cellY){
        Push p;
        if(!std::isfinite(now)||!std::isfinite(x)||!std::isfinite(y)||
           !std::isfinite(area)||!std::isfinite(cellX)||!std::isfinite(cellY)||cellX<=0||cellY<=0)return p;
        if(area<1.5f){if(lastSeen>=0&&now-lastSeen>.25f)active=false;return p;}
        if(!active||now-lastSeen>.45f||now<lastSeen){anchor(now,x,y,area);return p;}
        lastSeen=now;
        float elapsed=now-start,dx=x-ax,dy=y-ay;
        float distance=std::hypot(dx*cellX,dy*cellY);
        float reach=(std::sqrt(area)+std::sqrt(weight))*1.2f+2.f;
        if(area>weight*4||weight>area*4||std::hypot(dx,dy)>reach||elapsed>.65f){anchor(now,x,y,area);return p;}
        if(elapsed<.04f||now<next||distance<6.f||std::hypot(dx,dy)<.3f)return p;
        float speed=distance/elapsed;if(speed<45.f)return p;
        float gain=std::max(0.f,std::min(1.f,(speed-45.f)/455.f));gain=gain*gain*(3-2*gain);
        p.fire=true;p.x=x;p.y=y;p.dx=dx*cellX;p.dy=dy*cellY;p.speed=speed;
        p.radius=std::max(1.5f,std::min(7.f,std::sqrt(std::min(area,weight))*.5f+gain*2.f));
        p.strength=.35f+.65f*gain;
        anchor(now,x,y,area);next=now+.20f;return p;
    }
private:
    bool active=false;float ax=0,ay=0,weight=0,start=0,lastSeen=-1,next=0;
    void anchor(float now,float x,float y,float area){active=true;ax=x;ay=y;weight=area;start=lastSeen=now;}
};
