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
#include <vector>

// Shared on the application/render thread only. No camera reads, callbacks,
// retained Volcano pointers, or changes to the fluid simulation.
namespace MagicSandTreeFire {
struct ContactMap {
    float x=0,y=0,width=0,height=0,publishedAt=-1000;
    int columns=0,rows=0;
    unsigned generation=0;
    std::vector<unsigned char> hot;
    void clear() {hot.clear();columns=rows=0;publishedAt=-1000;++generation;}
    void publish(float px,float py,float w,float h,int cols,int lines,
                 const std::vector<float>& lava,const std::vector<float>& energy,
                 const std::vector<unsigned char>& valid,float now) {
        x=px;y=py;width=w;height=h;columns=cols;rows=lines;publishedAt=now;
        hot.resize(lava.size());
        for(size_t i=0;i<lava.size();++i)
            hot[i]=valid[i]&&lava[i]>.15f&&energy[i]/lava[i]>.25f;
    }
    bool touches(float px,float py,float radius,float now) const {
        if(columns<1||rows<1||width<=0||height<=0||now-publishedAt>.75f||now<publishedAt)return false;
        float sx=width/columns,sy=height/rows;
        int x0=std::max(0,int(std::floor((px-radius-x)/sx)));
        int x1=std::min(columns-1,int(std::floor((px+radius-x)/sx)));
        int y0=std::max(0,int(std::floor((py-radius-y)/sy)));
        int y1=std::min(rows-1,int(std::floor((py+radius-y)/sy)));
        for(int iy=y0;iy<=y1;++iy)for(int ix=x0;ix<=x1;++ix) {
            if(!hot[iy*columns+ix])continue;
            float nearestX=std::max(x+ix*sx,std::min(x+(ix+1)*sx,px));
            float nearestY=std::max(y+iy*sy,std::min(y+(iy+1)*sy,py));
            float dx=px-nearestX,dy=py-nearestY;
            if(dx*dx+dy*dy<=radius*radius)return true;
        }
        return false;
    }
};
inline ContactMap& contactMap() {static ContactMap map;return map;}
struct Burn {
    float started=-1;
    void update(bool touching,float now) {if(started<0&&touching)started=now;}
    float age(float now) const {return started<0 ? -1 : std::max(0.0f,now-started);}
    bool burning(float now) const {float t=age(now);return t>=0&&t<14;}
    float charAmount(float now) const {float t=age(now);return t<0 ? 0 : std::min(1.0f,t/12);}
};
}
