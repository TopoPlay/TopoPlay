#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// Independent lava state: never changes the rainwater solver or its pools.
// Equal-area cells; ground and fluid/solid depths are in millimetres.
class VolcanoGrid {
public:
    int width=0,height=0;
    std::vector<float> ground,lava,energy,crust;
    std::vector<unsigned char> valid;
    float coolingSeconds=16.0f,transitSeconds=.85f;
    void setup(int w,int h) {
        width=w;height=h;int n=w*h;
        ground.assign(n,0);lava.assign(n,0);energy.assign(n,0);
        crust.assign(n,0);valid.assign(n,0);
        delta.assign(n,0);heatDelta.assign(n,0);out.assign(n*4,0);
    }
    void clear() {
        std::fill(lava.begin(),lava.end(),0);
        std::fill(energy.begin(),energy.end(),0);
        std::fill(crust.begin(),crust.end(),0);
    }
    float total() const {float s=0;for(size_t i=0;i<lava.size();++i)s+=lava[i]+crust[i];return s;}
    float mobileTotal() const {float s=0;for(float v:lava)s+=v;return s;}
    float heat(int i) const {return lava[i]>.00001f ? std::max(0.0f,std::min(1.0f,energy[i]/lava[i])) : 0;}
    void add(int i,float depth) {
        if(i>=0 && i<static_cast<int>(lava.size()) && valid[i] && depth>0) {
            lava[i]+=depth;energy[i]+=depth;
        }
    }
    void step(float dt) {
        if(width<2||height<2||dt<=0)return;
        dt=std::min(dt,.025f);
        const float transferFraction=1-std::exp(-dt/transitSeconds);
        const float coolingFactor=std::exp(-dt/coolingSeconds);
        const float solidFraction=1-std::exp(-dt/2.5f);
        std::fill(delta.begin(),delta.end(),0);
        std::fill(heatDelta.begin(),heatDelta.end(),0);
        std::fill(out.begin(),out.end(),0);
        const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            int i=y*width+x;if(!valid[i]||lava[i]<=0)continue;
            float temperature=heat(i),sum=0;
            float level=ground[i]+crust[i]+lava[i];
            float mobility=.12f+.88f*temperature*temperature;
            for(int d=0;d<4;++d) {
                int xx=x+dx[d],yy=y+dy[d];if(xx<0||yy<0||xx>=width||yy>=height)continue;
                int j=yy*width+xx;if(!valid[j])continue;
                float head=level-ground[j]-crust[j]-lava[j];
                out[i*4+d]=std::max(0.0f,head)*1.6f*mobility*dt;sum+=out[i*4+d];
            }
            float available=lava[i]*transferFraction;
            float scale=sum>available ? available/sum : 1;
            for(int d=0;d<4;++d)out[i*4+d]*=scale;
        }
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            int i=y*width+x;if(!valid[i])continue;
            float temperature=heat(i);
            for(int d=0;d<4;++d) {
                float q=out[i*4+d];if(q<=0)continue;
                int j=(y+dy[d])*width+x+dx[d];
                delta[i]-=q;delta[j]+=q;
                heatDelta[i]-=q*temperature;heatDelta[j]+=q*temperature;
            }
        }
        for(size_t i=0;i<lava.size();++i) {
            if(lava[i]==0&&delta[i]==0&&energy[i]==0)continue;
            lava[i]=std::max(0.0f,lava[i]+delta[i]);
            energy[i]=std::max(0.0f,energy[i]+heatDelta[i])*coolingFactor;
            if(lava[i]<=.000001f){crust[i]+=lava[i];lava[i]=0;energy[i]=0;continue;}
            float temperature=heat(i);
            if(temperature<.32f) {
                float solid=lava[i]*solidFraction;
                lava[i]-=solid;crust[i]+=solid;energy[i]=lava[i]*temperature;
                // Below 0.005 mm and cold: visually invisible mobile film.
                // Preserve its exact volume as crust, allowing the solver to sleep.
                if(lava[i]<.005f){crust[i]+=lava[i];lava[i]=0;energy[i]=0;}
            }
        }
    }
private:
    std::vector<float> delta,heatDelta,out;
};

// Three stable seconds to arm a five-second warning, one eruption per mound.
class VolcanoTrigger {
public:
    bool latched=false,warning=false,erupting=false;
    int vent=-1;
    float stableSince=-1,warningAt=-1,eruptUntil=-1,lastTime=-1;
    void reset(){*this=VolcanoTrigger();}
    // result: 1 warning began, 2 eruption began, -1 warning cancelled, 0 no event.
    int update(float now,bool allowed,bool clear,bool mound,bool lowered,int peak) {
        if(!allowed){reset();return 0;}
        if(lastTime>=0 && now-lastTime>.65f) stableSince=-1;
        lastTime=now;
        if(erupting && now>=eruptUntil)erupting=false;
        if(!warning&&!erupting&&latched&&clear&&lowered){latched=false;vent=-1;}
        if(warning) {
            if(!clear||!mound||peak!=vent) {
                warning=false;stableSince=-1;vent=-1;return -1;
            }
            if(now-warningAt>=5) {
                warning=false;erupting=true;latched=true;eruptUntil=now+30;return 2;
            }
            return 0;
        }
        if(latched||erupting)return 0;
        if(!clear||!mound){stableSince=-1;vent=-1;return 0;}
        if(vent!=peak){vent=peak;stableSince=now;}
        if(stableSince<0)stableSince=now;
        if(now-stableSince>=3){warning=true;warningAt=now;return 1;}
        return 0;
    }
};
