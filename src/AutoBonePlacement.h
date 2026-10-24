/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
// AUTO_BONE_V22: a single shared live layout drives sensing and drawing.
namespace MagicSandAutoBone {
struct Rect {float x=0,y=0,w=0,h=0;};
// x/y are camera depth pixels; px/py and rx/ry are projector pixels.
// depth is camera world Z in millimetres, not elevation above sea level.
// Drawing and detection share the chosen candidate to avoid independent
// corner searches selecting different physical points.
struct Candidate {float x=0,y=0,px=0,py=0,rx=0,ry=0,depth=0;bool valid=false;};
struct State {
    Rect roi;float width=0,height=0,sensorWidth=0,sensorHeight=0,radius=0;
    float nextCheck=-1000,invalidSince=-1;bool initialized=false,ready=false;
    unsigned revision=0;const void* owner=nullptr;Candidate chosen;
    std::vector<float> mapping;
};
inline State& current(){static State s;return s;}
inline bool same(Rect a,Rect b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
inline bool sameMapping(const std::vector<float>& a,const std::vector<float>& b){
    if(a.size()!=b.size())return false;
    for(size_t n=0;n<a.size();++n)if(!std::isfinite(a[n])||!std::isfinite(b[n])||std::abs(a[n]-b[n])>.25f)return false;
    return true;
}
inline bool fits(const Candidate& c,float width,float height){
    if(!c.valid||!std::isfinite(c.px)||!std::isfinite(c.py)||!std::isfinite(c.rx)||!std::isfinite(c.ry)||c.rx<=0||c.ry<=0)return false;
    // BONE_CORNER_V24: reserve artwork and a small-ring margin, rather than
    // a full large hand-ring radius. Recognition still uses the raw camera.
    float mx=std::max(14.f,c.rx*.45f+8),my=std::max(14.f,c.ry*.45f+8);
    return c.px>=mx&&c.py>=my&&c.px<=width-mx&&c.py<=height-my;
}
template<class Probe> State& update(Rect roi,float width,float height,float sensorWidth,float sensorHeight,
    float radius,float now,const void* owner,const std::vector<float>& mapping,Probe probe,State* separateState=nullptr,bool oppositeCorner=false){
    State& s=separateState?*separateState:current();
    bool geometry=!s.initialized||!same(s.roi,roi)||s.width!=width||s.height!=height||
        s.sensorWidth!=sensorWidth||s.sensorHeight!=sensorHeight||s.radius!=radius||s.owner!=owner||!sameMapping(s.mapping,mapping);
    if(!geometry&&now<s.nextCheck)return s;
    // Placement is checked at 4 Hz to limit inverse-projection work. An existing
    // valid location is reused so normal depth noise does not make it wander.
    s.nextCheck=now+.25f;
    if(geometry){s.roi=roi;s.width=width;s.height=height;s.sensorWidth=sensorWidth;s.sensorHeight=sensorHeight;
        s.radius=radius;s.owner=owner;s.mapping=mapping;s.initialized=true;s.ready=false;s.invalidSince=-1;++s.revision;}
    if(width<=0||height<=0||roi.w<100||roi.h<100)return s;
    for(float v:mapping)if(!std::isfinite(v)){s.ready=false;return s;}
    if(!geometry&&s.chosen.valid){
        Candidate c=probe(s.chosen.x,s.chosen.y);
        if(fits(c,width,height)){s.chosen=c;s.ready=true;s.invalidSince=-1;return s;}
        s.ready=false;
        if(s.invalidSince<0)s.invalidSince=now;
        // Do not chase a hand or jump due to a brief bad depth frame.
        if(now-s.invalidSince<.75f)return s;
    }
    const float inset=std::max(6.f,std::min(10.f,radius*.35f));
    const float cameraMargin=radius+4;
    float left=std::max(roi.x+inset,cameraMargin),right=std::min(roi.x+roi.w-inset,sensorWidth-1-cameraMargin);
    float top=std::max(roi.y+inset,cameraMargin),bottom=std::min(roi.y+roi.h-inset,sensorHeight-1-cameraMargin);
    if(left>right||top>bottom){s.ready=false;return s;}
    std::vector<Candidate> candidates;
    auto collect=[&](int cells){
        for(int j=0;j<=cells;++j)for(int i=0;i<=cells;++i){
            Candidate c=probe(left+(right-left)*i/cells,top+(bottom-top)*j/cells);
            if(c.valid&&std::isfinite(c.px)&&std::isfinite(c.py)&&c.px>=0&&c.py>=0&&c.px<=width&&c.py<=height)candidates.push_back(c);
        }
    };
    collect(14);
    auto choose=[&]()->bool{
        float minX=width,minY=height,maxX=0,maxY=0;bool found=false;
        for(const auto& c:candidates){minX=std::min(minX,c.px);minY=std::min(minY,c.py);maxX=std::max(maxX,c.px);maxY=std::max(maxY,c.py);}
        const float spanX=std::max(1.f,maxX-minX),spanY=std::max(1.f,maxY-minY);
        // BONE_CORNER_V24: prefer the upper-left visible corner, with existing safety margins.
        const float targetX=oppositeCorner?maxX:minX,targetY=oppositeCorner?maxY:minY;
        float best=std::numeric_limits<float>::infinity();Candidate selected;
        for(const auto& c:candidates){if(!fits(c,width,height))continue;
            float dx=(c.px-targetX)/spanX,dy=(c.py-targetY)/spanY;
            float score=4*dx*dx+dy*dy;
            if(score<best){best=score;selected=c;found=true;}
        }
        if(found){bool moved=!s.chosen.valid||std::abs(selected.x-s.chosen.x)>.01f||std::abs(selected.y-s.chosen.y)>.01f;
            s.chosen=selected;s.ready=true;s.invalidSince=-1;if(moved)++s.revision;}
        return found;
    };
    if(!choose()){candidates.clear();collect(30);if(!choose()){s.ready=false;s.chosen.valid=false;}}
    return s;
}
}
