/* TopoPlay contributor credit — 2026-10-25
 * Todd Taylor and Grayson Taylor — TopoPlay development and testing.
 * This credit applies to TopoPlay contributions; original authors retain
 * credit for their work. Existing copyright and license notices are preserved.
 */
/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "WaterRippleGrid.h"
#include <cstdint>
// BACKGROUND_RIPPLES_V76: reuse the existing signed ripple simulation.
// No water volume, hydraulic momentum, runoff, or terrain is modified here.
class AmbientWaterRipples {
public:
    void reset(){next=-1;}
    bool update(float now,WaterRippleGrid& waves,const std::vector<float>& depth){
        if(!std::isfinite(now)||waves.width<5||waves.height<5||depth.size()!=waves.wet.size())return false;
        if(next<0){next=now+1.f;return false;}
        if(now<next)return false;
        next=now+1.8f+unit()*2.f;
        int chosen=-1;float directionX=0,directionY=0;
        const int w=waves.width,h=waves.height;
        for(int tries=0;tries<40;++tries){
            int i=int(random()%waves.wet.size()),x=i%w,y=i/w;
            if(x<2||y<2||x>=w-2||y>=h-2||!waves.wet[i]||depth[i]<1.5f)continue;
            if(!waves.wet[i-1]||!waves.wet[i+1]||!waves.wet[i-w]||!waves.wet[i+w])continue;
            if(chosen<0)chosen=i; // enclosed pools can use an interior source too
            bool edge=false;
            for(int d=2;d<=5&&!edge;++d){
                const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
                for(int side=0;side<4;++side){
                    int xx=x+dx[side]*d,yy=y+dy[side]*d;
                    if(xx<0||yy<0||xx>=w||yy>=h||!waves.wet[yy*w+xx]){
                        chosen=i;directionX=-dx[side];directionY=-dy[side];edge=true;break;
                    }
                }
            }
            if(edge)break;
        }
        if(chosen<0)return false;
        float angle=unit()*6.2831853f;
        if(directionX==0&&directionY==0){directionX=std::cos(angle);directionY=std::sin(angle);}
        else{directionX+=std::cos(angle)*.45f;directionY+=std::sin(angle)*.45f;}
        float wetGain=std::max(0.f,std::min(1.f,(depth[chosen]-1.5f)/6.5f));
        float strength=(.10f+.06f*unit())*(.35f+.65f*wetGain);
        float radius=1.8f+unit()*1.2f;
        // Same smooth crest/trough packet used by hand input; varied location,
        // direction, size and spacing avoid a tiled or repeating light pattern.
        waves.push(chosen%w+.5f,chosen/w+.5f,directionX,directionY,radius,strength);
        return true;
    }
private:
    float next=-1;uint32_t state=0x583719u;
    uint32_t random(){state=state*1664525u+1013904223u;return state;}
    float unit(){return float(random()>>8)*(1.f/16777216.f);}
};
