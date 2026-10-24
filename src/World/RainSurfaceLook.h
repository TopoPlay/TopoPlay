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
// POOL_TEXTURE_V28: visible thin blue film and depth-blended pool layer.
// Appearance only; never writes physical water or changes rain rate.
class RainSurfaceLook {
public:
    struct Color{float r=0,g=0,b=0,a=0;};
    int width=0,height=0;
    std::vector<float> wetness,shownDepth;
    std::vector<Color> corners,poolCorners;
    void setup(int w,int h){width=w;height=h;wetness.assign(w*h,0);shownDepth.assign(w*h,0);corners.assign((w+1)*(h+1),Color{});poolCorners.assign((w+1)*(h+1),Color{});visible_=false;}
    bool visible()const{return visible_;}
    static float blend(float a,float b,float v){float x=std::max(0.f,std::min(1.f,(v-a)/(b-a)));return x*x*(3-2*x);}
    static Color shade(float wet,float){
        // Stronger blue conveys the downpour without adding hydraulic volume.
        return {.045f,.39f,.82f,.34f*std::max(0.f,std::min(1.f,wet))};
    }
    static Color poolShade(float depth){
        // Water depth chooses the material, not elapsed storm time.
        // Broad transition avoids an abrupt, moving blue front on thin runoff.
        const float pooled=blend(.7f,2.8f,depth);
        return {.10f,.58f,.94f,.88f*pooled};
    }
    void update(float dt,const std::vector<float>& depth,const std::vector<unsigned char>& valid,
                const std::vector<unsigned char>& rain,float rate){
        if(width<2||height<2||depth.size()!=wetness.size()||valid.size()!=depth.size())return;
        dt=std::max(0.f,std::min(.1f,dt));
        const float wetRise=1-std::exp(-dt/.8f),wetFall=1-std::exp(-dt/8.f);
        const float depthRise=1-std::exp(-dt/.65f),depthFall=1-std::exp(-dt/.20f);
        for(size_t i=0;i<depth.size();++i){
            if(!valid[i]||!std::isfinite(depth[i])){wetness[i]=shownDepth[i]=0;continue;}
            const float d=std::max(0.f,depth[i]);
            float target=1-std::exp(-d/.35f);
            if(i<rain.size()&&rain[i]&&std::isfinite(rate)&&rate>0)
                target=std::max(target,std::max(.75f,std::min(1.f,rate/.3f))); // visible from the first positive rain
            wetness[i]+=(target-wetness[i])*(target>wetness[i]?wetRise:wetFall);
            // Fast removal and slower filling damp numerical color fronts
            // without leaving an invented deep pool on screen after drainage.
            shownDepth[i]+=(d-shownDepth[i])*(d>shownDepth[i]?depthRise:depthFall);
            if(wetness[i]<.0001f)wetness[i]=0;
            if(shownDepth[i]<.0001f)shownDepth[i]=0;
        }
        visible_=false;
        for(int y=0;y<=height;++y)for(int x=0;x<=width;++x){
            float wet=0,d=0;int count=0;
            for(int yy=y-1;yy<=y;++yy)for(int xx=x-1;xx<=x;++xx){
                if(xx<0||yy<0||xx>=width||yy>=height)continue;
                const int i=yy*width+xx;if(!valid[i])continue;
                wet+=wetness[i];d+=shownDepth[i];++count;
            }
            Color c=count?shade(wet/count,d/count):Color{};
            Color p=count?poolShade(d/count):Color{};
            const int corner=y*(width+1)+x;
            corners[corner]=c;poolCorners[corner]=p;
            visible_=visible_||c.a>.002f||p.a>.002f;
        }
    }
private:
    bool visible_=false;
};
