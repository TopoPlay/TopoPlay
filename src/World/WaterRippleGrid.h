/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#pragma once
// V67_WATER_RETURN_V72: pre-10x ripple dynamics; broad V70 absorber removed.
// WATER_CREDITS_V69: the adapted Evan Wallace ripple update retains its MIT
// copyright and permission notice in licenses/evan-water-MIT.txt. The source
// repository and the boundary between ripple detail and hydraulic flow are
// documented in docs/WATER-MOVEMENT.md. This notice does not replace the
// original Magic-Sand/AR Sandbox licenses or claim their work is MIT-licensed.
/*
 * Water ripple update adapted from Evan Wallace's WebGL Water, water.js.
 * Copyright 2011 Evan Wallace. MIT license: see licenses/evan-water-MIT.txt.
 * https://github.com/evanw/webgl-water
 * Adaptation: small CPU grid, wet-cell boundaries, bounded fixed timestep.
 * These signed surface offsets NEVER enter the rain volume/drainage solver.
 */
#include <vector>
#include <algorithm>
#include <cmath>
class WaterRippleGrid {
public:
    int width=0,height=0;
    std::vector<float> displacement,velocity;
    std::vector<unsigned char> wet;
    std::vector<float> propagation,damping,east,south;
    bool weighted=false;
    void setup(int w,int h){
        width=std::max(2,std::min(64,w));height=std::max(2,std::min(64,h));
        displacement.assign(width*height,0);velocity=displacement;
        next=displacement;wet.assign(width*height,0);
        propagation.assign(width*height,1);damping.assign(width*height,.985f);
        east=next;south=next;weighted=false;
    }
    void disturb(int i,float amount){
        if(i<0||i>=int(wet.size())||!wet[i]||!std::isfinite(amount))return;
        displacement[i]=clamp(displacement[i]+amount,-1.5f,1.5f);
    }
    // SHORE_WAVES_V64: cached symmetric face weights for a visual wave model.
    // This is NOT a volume-conserving shallow-water/flood solver. Bed/depth here
    // choose propagation and absorption; rain storage remains in RainWaterGrid.
    void configureShore(const std::vector<float>& bed,const std::vector<float>& depth,
                        const std::vector<unsigned char>& valid){
        const int n=width*height;
        if(bed.size()!=size_t(n)||depth.size()!=size_t(n)||valid.size()!=size_t(n))return;
        weighted=true;
        for(int i=0;i<n;++i){
            float d=std::isfinite(depth[i])?std::max(0.f,depth[i]):0;
            propagation[i]=std::sqrt(clamp(d/20.f,.15f,1.f));
            damping[i]=.985f;
        }
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            int i=y*width+x;east[i]=south[i]=0;
            if(!wet[i])continue;
            auto face=[&](int j){float c=std::min(propagation[i],propagation[j]);return wet[j]?c*c:0.f;};
            if(x+1<width)east[i]=face(i+1);
            if(y+1<height)south[i]=face(i+width);
            float loss=0;const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
            for(int q=0;q<4;++q){
                int xx=x+dx[q],yy=y+dy[q];
                if(xx<0||yy<0||xx>=width||yy>=height)continue;
                int j=yy*width+xx;
                if(!wet[j]&&valid[j]){
                    float rise=std::max(0.f,bed[j]-(bed[i]+depth[i]));
                    float hard=clamp(rise/30.f,0.f,1.f);
                    // A low beach absorbs more than a tall, steep bank. Dry
                    // faces have zero transmission in both cases: no land waves.
                    loss+=.003f+.06f*(1-hard);
                }
            }
            // V73: only cells directly touching a dry bank absorb visual
            // echoes. Interior hand ripples retain their original damping.
            damping[i]=loss>0?.35f:.985f;
        }
    }
    // A broad crest/trough pair plus matching initial vertical velocity launches
    // a packet predominantly along the sweep instead of a circular impact.
    void push(float cx,float cy,float dx,float dy,float radius,float strength){
        if(!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(dx)||!std::isfinite(dy)||
           !std::isfinite(radius)||!std::isfinite(strength))return;
        float length=std::sqrt(dx*dx+dy*dy);if(length<.001f)return;
        dx/=length;dy/=length;radius=clamp(radius,1.5f,7.f);strength=clamp(strength,0.f,.95f);
        int extent=int(std::ceil(radius*2.2f));
        for(int y=std::max(0,int(cy)-extent);y<std::min(height,int(cy)+extent+1);++y)
            for(int x=std::max(0,int(cx)-extent);x<std::min(width,int(cx)+extent+1);++x){
                int i=y*width+x;if(!wet[i])continue;
                float px=x+.5f-cx,py=y+.5f-cy;
                float along=(px*dx+py*dy)/radius,across=(-px*dy+py*dx)/(radius*1.5f);
                auto bell=[&](float u){float t=std::max(0.f,1-u*u-across*across);return t*t;};
                auto derivative=[&](float u){float t=std::max(0.f,1-u*u-across*across);return -4*u*t/(radius*.6f);};
                float a=(along-.7f)/.6f,b=(along+.5f)/.6f;
                disturb(i,strength*(bell(a)-bell(b)));
                float c=weighted?propagation[i]:1.f;
                velocity[i]=clamp(velocity[i]-.47434165f*c*strength*(derivative(a)-derivative(b)),-.8f,.8f);
            }
    }
    void step(float speed=1.f){
        // CALM_WATER_V62: a fractional visual timestep slows propagation while
        // keeping the 30Hz caller cadence. Damping stays per tick so slower waves
        // do not accumulate into long-lived interference. Default 1 preserves
        // the original v60/v61 solver for other callers.
        speed=std::isfinite(speed)?clamp(speed,.1f,1.f):1.f;
        // Evan's average-neighbour acceleration and damped vertical velocity.
        // Reflect at dry banks instead of transmitting ripples through land.
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            int i=y*width+x;
            if(!wet[i]){next[i]=velocity[i]=0;continue;}
            float center=displacement[i];
            auto neighbour=[&](int xx,int yy){
                if(xx<0||yy<0||xx>=width||yy>=height)return center;
                int j=yy*width+xx;return wet[j]?displacement[j]:center;
            };
            float average=weighted?center:.25f*(neighbour(x-1,y)+neighbour(x+1,y)+neighbour(x,y-1)+neighbour(x,y+1));
            if(weighted){
                float flux=0;
                if(x>0)flux+=east[i-1]*(displacement[i-1]-center);
                if(x+1<width)flux+=east[i]*(displacement[i+1]-center);
                if(y>0)flux+=south[i-width]*(displacement[i-width]-center);
                if(y+1<height)flux+=south[i]*(displacement[i+width]-center);
                average=center+flux*.25f;
            }
            velocity[i]=(velocity[i]+(average-center)*.9f*speed)*(weighted?damping[i]:.985f);
            next[i]=clamp(center+velocity[i]*speed,-1.5f,1.5f);
        }
        displacement.swap(next);
    }
    float slope(int x,int y,int dx,int dy)const{
        int i=y*width+x;
        if(!wet[i])return 0;
        auto value=[&](int xx,int yy){
            if(xx<0||yy<0||xx>=width||yy>=height)return displacement[i];
            int j=yy*width+xx;return wet[j]?displacement[j]:displacement[i];
        };
        return .5f*(value(x+dx,y+dy)-value(x-dx,y-dy));
    }
private:
    std::vector<float> next;
    static float clamp(float v,float lo,float hi){return std::max(lo,std::min(hi,v));}
};
