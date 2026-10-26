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
// WATER_CREDITS_V69: this project-specific runoff/surge solver is separate
// from Evan Wallace's MIT-licensed surface-ripple update in WaterRippleGrid.h.
// Its existing GPL-2.0-or-later notice and numerical references are retained.
// SHORE_STORAGE_V73: incident surge enters land storage; return flow is overdamped.
// V67_WATER_RETURN_V72: V67 dynamics restored; see docs/v67-water-return-v72.md.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <vector>
#include <algorithm>
#include <cmath>

// SETTLE_WATER_V67: finite-time decay of surge momentum, without removing water.
// SURGE_WATER_V66: conservative storage with persistent face discharge.
// Project-specific local-inertial discretization; no third-party source copied.
// Method: Bates et al. 2010, doi:10.1016/j.jhydrol.2010.03.027;
// de Almeida et al. 2012, doi:10.1029/2011WR011570.
// This approximation retains local acceleration, not convective acceleration:
// useful for sandbox surges, not an engineering dam-break/tsunami predictor.
class RainWaterGrid {
public:
    int width=0,height=0;
    std::vector<float> ground,water,flowX,flowY;
    std::vector<unsigned char> valid;
    float rainRate=1.25f,evaporation=.16f,edgeDrain=.8f;
    void setCellSize(float x,float y){if(std::isfinite(x)&&x>=2&&x<=100)cellX=x;if(std::isfinite(y)&&y>=2&&y<=100)cellY=y;}
    float cellWidthMm()const{return cellX;}
    float cellHeightMm()const{return cellY;}
    void terrainChanged(int i,float before,float after){if(i>=0&&i<int(shoreWater.size())&&after<before-.5f)shoreWater[i]=0;} // A changed bed changes head on the next pass; water can escape the raised bed.
    void setup(int w,int h){
        width=std::max(0,w);height=std::max(0,h);int n=width*height;
        ground.assign(n,0);water=ground;flowX=ground;flowY=ground;
        valid.assign(n,0);display=ground;base=ground;qx=ground;qy=ground;head=ground;
        out=ground;delta=ground;scale=ground;fx=ground;fy=ground;incidentX=ground;incidentY=ground;quietX.assign(n,0);quietY.assign(n,0);shoreWater.assign(n,0);quietUntil=ground;pushUntilX=ground;pushUntilY=ground;clock=0;
        sea=false;seaLevel=0;lastSubsteps=0;
    }
    // Regular-mode sea is an initial, level body of water. Reconcile only its
    // geometric baseline when the bed/profile changes; never reset a wave each
    // frame. Rain and run-up then share the same conserved storage/discharges.
    void configureSea(bool enabled,float level){
        if(!std::isfinite(level))enabled=false;
        sea=enabled;seaLevel=enabled?level:0;
        for(size_t i=0;i<water.size();++i){
            float b=sea&&valid[i]&&std::isfinite(ground[i])?std::max(0.f,seaLevel-ground[i]):0;
            water[i]=valid[i]?std::max(0.f,water[i]+b-base[i]):0;
            base[i]=b;
        }
    }
    void clear(){
        for(auto* v:{&water,&base,&qx,&qy,&flowX,&flowY})std::fill(v->begin(),v->end(),0);
        sea=false;std::fill(shoreWater.begin(),shoreWater.end(),0);std::fill(quietX.begin(),quietX.end(),0);std::fill(quietY.begin(),quietY.end(),0);
    }
    float total()const{double s=0;for(float h:water)s+=h;return float(s);}
    // The original film renderer must not paint a permanent blue rain film
    // over the newly explicit regular sea. Its terrain material already exists.
    const std::vector<float>& displayWater(){
        for(size_t i=0;i<water.size();++i)display[i]=base[i]>0?0:water[i];
        return display;
    }
    float baseline(int i)const{return i>=0&&i<int(base.size())?base[i]:0;}
    bool hasDynamics()const{
        for(size_t i=0;i<water.size();++i)if(valid[i]&&(std::abs(water[i]-base[i])>.001f||std::abs(qx[i])>.05f||std::abs(qy[i])>.05f))return true;
        return false;
    }
    // Original slow-film law is retained for sub-millimetre rain saturation.
    static float speed(float h,float slope){
        if(h<=0||slope<=0)return 0;
        return std::min(9.81f*h*h*slope/(3.e-6f),std::pow(h,2.f/3.f)*std::sqrt(slope)/.12f);
    }
    // A push supplies momentum, never water. Coordinates/radius are in this
    // grid's cells; direction components are physical (metric) directions.
    bool push(float cx,float cy,float dx,float dy,float radius,float strength){
        if(!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(radius)||!std::isfinite(strength))return false;
        float length=std::sqrt(dx*dx+dy*dy);if(length<.001f)return false;
        dx/=length;dy/=length;radius=clamp(radius,1.5f,14.f);strength=clamp(strength,0.f,1.f);bool hit=false;
        for(int y=std::max(0,int(cy-radius*2));y<std::min(height,int(cy+radius*2+1));++y)
        for(int x=std::max(0,int(cx-radius*2));x<std::min(width,int(cx+radius*2+1));++x){
            int i=y*width+x;if(!valid[i]||water[i]<.5f)continue;
            auto kick=[&](int j,float px,float py,float component,float& q){
                if(j>=int(water.size())||!valid[j])return;
                float along=((px-cx)*dx+(py-cy)*dy)/radius;
                float across=(-(px-cx)*dy+(py-cy)*dx)/(radius*1.3f);
                float t=std::max(0.f,1-along*along-across*across);
                float h=std::max(0.f,std::max(ground[i]+water[i],ground[j]+water[j])-std::max(ground[i],ground[j]));
                if(h<.5f||t<=0)return;
                // Bounded hand shove: deeper virtual water stores more discharge.
                q+=component*7.f*strength*h*std::sqrt(9810.f*h)*t*t;
                q=clamp(q,-3*h*std::sqrt(9810.f*h),3*h*std::sqrt(9810.f*h));
                // Deliberate new input wins over an earlier shore's return zone.
                if(&q==&qx[i])pushUntilX[i]=clock+.6f;else pushUntilY[i]=clock+.6f;
                hit=true;
            };
            if(x+1<width)kick(i+1,x+1.f,y+.5f,dx,qx[i]);
            if(y+1<height)kick(i+width,x+.5f,y+1.f,dy,qy[i]);
        }
        return hit;
    }
    int lastSubsteps=0;
    void step(float dt,const std::vector<unsigned char>& rain){
        if(width<2||height<2||!std::isfinite(dt)||dt<=0)return;
        dt=std::min(dt,.025f);clock+=dt;int n=width*height;
        for(int i=0;i<n;++i){
            if(clock>=quietUntil[i])quietX[i]=quietY[i]=0;
            if(!valid[i]||!std::isfinite(ground[i])){water[i]=qx[i]=qy[i]=0;continue;}
            if(!std::isfinite(water[i])||water[i]<0)water[i]=0;
            if(i<int(rain.size())&&rain[i]&&std::isfinite(rainRate)&&rainRate>0)water[i]+=rainRate*dt;
        }
        // Hydraulic time is slowed to 35% for legible sandbox waves. Rain and
        // five-minute pond evaporation use real time, independent of this scale.
        // A CFL scan caps work to eight small passes; extreme tiny-cell/deep-water
        // setups slow transport further rather than increasing CPU without bound.
        float maxH=0;
        for(int i=0;i<n;++i)if(valid[i])maxH=std::max(maxH,water[i]);
        float limit=.24f*std::min(cellX,cellY)/std::sqrt(9810.f*std::max(.1f,maxH));
        float transport=std::min(dt*.35f,limit*8);
        int passes=std::max(1,std::min(8,int(std::ceil(transport/limit))));lastSubsteps=passes;
        float sub=transport/passes;
        // Manning friction becomes weak in deep water and weakens further as
        // velocity falls. Add linear unresolved mixing/bank drag so reflected
        // surges settle rather than circulating almost indefinitely. Decay uses
        // real elapsed time, independent of hydraulic slowdown/CFL pass count.
        // Only stored momentum decays: pressure still levels/drains water.
        const float momentumDecay=std::exp(-(dt/passes)/3.5f);
        std::fill(flowX.begin(),flowX.end(),0);std::fill(flowY.begin(),flowY.end(),0);
        for(int pass=0;pass<passes;++pass){
            incidentX=qx;incidentY=qy;
            for(int i=0;i<n;++i)head[i]=ground[i]+water[i];
            std::fill(out.begin(),out.end(),0);std::fill(delta.begin(),delta.end(),0);
            auto face=[&](int i,int j,float distance,float& q){
                if(!valid[i]||!valid[j]||!std::isfinite(ground[i])||!std::isfinite(ground[j])){q=0;return 0.f;}
                float dh=head[i]-head[j];
                // A shoreline face consumes the incident discharge into real
                // water on the higher bed. The momentum is spent climbing and
                // breaking; it is NOT stored as a spring that reflects offshore.
                // No distant cells are damped, and no water is manufactured.
                bool shore=sea && ((base[i]>0 && ground[j]>=seaLevel) ||
                                  (base[j]>0 && ground[i]>=seaLevel));
                if(shore){
                    int ocean=base[i]>0?i:j,bank=ocean==i?j:i;
                    float sign=ocean==i?1.f:-1.f;
                    float incoming=sign*q;
                    // Use the adjacent incident face too: a previously dry bank
                    // has no face discharge of its own to carry a crest uphill.
                    if(j==i+1){
                        int k=ocean==i?i-1:j;
                        if(k>=0 && k<width*height && k/width==ocean/width)incoming=std::max(incoming,sign*incidentX[k]);
                    }else{
                        int k=ocean==i?i-width:j;
                        if(k>=0 && k<width*height)incoming=std::max(incoming,sign*incidentY[k]);
                    }
                    float amount=0;
                    
                    if(incoming>0){
                        float velocity=incoming/std::max(1.f,water[ocean]);
                        float lift=std::min(100.f,velocity*velocity/(2*9810.f));
                        float room=std::max(0.f,head[ocean]+lift-head[bank]);
                        amount=std::min(water[ocean]*.5f,std::min(room*.5f,incoming*sub/distance));
                    }
                    // A flooded bank drains by head difference without inertia.
                    // Limit withdrawal per pass so it pulls back smoothly instead
                    // of launching another sharp offshore crest. A new incoming
                    // wave wins over drainage while it is climbing the bank.
                    if(amount<=0 && head[bank]>head[ocean] && water[bank]>0){
                        float drop=head[bank]-std::max(head[ocean],ground[bank]);
                        float depth=std::max(0.f,head[bank]-std::max(ground[ocean],ground[bank]));
                        amount=-std::min(water[bank]*(dt/passes)*.8f,
                            std::min(drop*.2f,speed(depth*.001f,drop/distance)*1000.f*depth*(dt/passes)/distance));
                    }
                    if(amount>0)shoreWater[bank]=1;
                    if(incoming>.1f || amount<0){
                        // Arm a return-only zone AFTER impact, not merely near
                        // land. Small ponds must still accept their first push.
                        int stride=j-i,step=ocean==i?-stride:stride;
                        unsigned bit=ocean==i?1:2;
                        for(int k=0;k<=8;++k){
                            int at=ocean+k*step;
                            if(at<0||at>=n||(stride==1&&at/width!=ocean/width)||base[at]<=0)break;
                            if(stride==1)quietX[at]|=bit;else quietY[at]|=bit;
                            quietUntil[at]=clock+4.f;
                        }
                    }
                    q=0;amount*=sign;
                    out[amount>=0?i:j]+=std::abs(amount);return amount;
                }
                float h=std::max(0.f,std::max(head[i],head[j])-std::max(ground[i],ground[j]));
                if(h<.001f){q=0;return 0.f;}
                float amount=0;
                // Absorb only momentum travelling BACK from a bank. The
                // incident direction is untouched, including a strong hand push.
                // Unlike the old wide absorber, the visual ripple grid and
                // shoreline-directed momentum remain responsive throughout.
                unsigned mask=(j==i+1?quietX[i]|quietX[j]:quietY[i]|quietY[j]);
                bool returning=sea&&base[i]>0&&base[j]>0&&
                    clock>=(j==i+1?pushUntilX[i]:pushUntilY[i])&&
                    (((mask&1)&&(q<0||(q==0&&dh<0))) ||
                     ((mask&2)&&(q>0||(q==0&&dh>0))));
                if(h<1.f || returning || (shoreWater[i]||shoreWater[j])){
                    // Thin rain retains v25's overdamped, slow downhill film.
                    q=0;int donor=dh>=0?i:j;
                    amount=speed(h*.001f,std::abs(dh)/distance)*1000.f*h*(dt/passes)/distance;
                    amount=std::min(amount,std::abs(dh)*.20f);if(dh<0)amount=-amount;
                    if(water[donor]<=0)amount=0;
                    if(returning)amount=clamp(amount,-water[donor]*(dt/passes)*.15f,water[donor]*(dt/passes)*.15f);
                }else{
                    // SI-unit Manning drag; q is stored as mm^2/s. Exponent
                    // 7/3 follows discharge per unit width, not cell volume.
                    float hm=h*.001f;
                    float rough=.025f+.095f*(1-blend(1.f,8.f,h));
                    float drag=1+9.81f*sub*rough*rough*std::abs(q*1.e-6f)/std::pow(hm,7.f/3.f);
                    q=(q*momentumDecay+9810.f*h*sub*dh/distance)/drag;
                    float cap=3*h*std::sqrt(9810.f*h);q=clamp(q,-cap,cap);
                    amount=q*sub/distance;
                }
                if(!std::isfinite(amount)){q=0;return 0.f;}
                out[amount>=0?i:j]+=std::abs(amount);return amount;
            };
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=y*width+x;
                fx[i]=x+1<width?face(i,i+1,cellX,qx[i]):0;
                fy[i]=y+1<height?face(i,i+width,cellY,qy[i]):0;
            }
            for(int i=0;i<n;++i)scale[i]=out[i]>water[i]&&out[i]>0?water[i]/out[i]:1;
            auto move=[&](int i,int j,float amount,float& q,float& fi,float& fj){
                float s=scale[amount>=0?i:j];amount*=s;q*=s;
                delta[i]-=amount;delta[j]+=amount;fi+=amount;fj+=amount;
                int donor=amount>=0?i:j,receiver=amount>=0?j:i;
                if(std::abs(amount)>.0001f&&shoreWater[donor]&&base[receiver]<=0)shoreWater[receiver]=1;
            };
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=y*width+x;
                if(x+1<width)move(i,i+1,fx[i],qx[i],flowX[i],flowX[i+1]);
                if(y+1<height)move(i,i+width,fy[i],qy[i],flowY[i],flowY[i+width]);
            }
            for(int i=0;i<n;++i)if(valid[i])water[i]=std::max(0.f,water[i]+delta[i]);
        }
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){int i=y*width+x;
            if(!valid[i]){water[i]=flowX[i]=flowY[i]=0;continue;}
            // Do not evaporate the regular sea. Rain/run-up above its baseline
            // keeps the previous pond loss policy; closed internal faces conserve.
            float excess=std::max(0.f,water[i]-base[i]);
            float loss=std::max(0.f,evaporation)*dt*std::min(1.f,excess)*(1-.8f*blend(.25f,1.f,excess));
            water[i]-=std::min(excess,loss);
            if(water[i]<.02f)shoreWater[i]=0;
            if(x==0||y==0||x==width-1||y==height-1){
                excess=std::max(0.f,water[i]-base[i]);water[i]-=excess*(1-std::exp(-std::max(0.f,edgeDrain)*dt));
            }
        }
    }
private:
    bool sea=false;float seaLevel=0,cellX=12,cellY=12,clock=0;
    std::vector<float> display,base,qx,qy,head,out,delta,scale,fx,fy,incidentX,incidentY,quietUntil,pushUntilX,pushUntilY;
    std::vector<unsigned char> quietX,quietY,shoreWater;
    static float clamp(float v,float a,float b){return std::max(a,std::min(b,v));}
    static float blend(float a,float b,float v){float t=clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
};
