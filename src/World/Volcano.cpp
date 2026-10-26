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
// SPDX-License-Identifier: GPL-2.0-or-later
#include "Volcano.h"
#include "TreeFire.h"
#include "LavaCover.h" // AUTO_VOLCANO_V1
#include <cmath>
#include <algorithm>
#include <limits>

void Volcano::setEnabled(bool allowed)
{
    if(armed==allowed)return;
    if(!demoMode)MagicSandTreeFire::contactMap().clear();MagicSandLavaCover::map().clear();belowResetSince=-1;
    volcanoSleeping=false;lastInteraction=-1;interactionHeights.clear();
    armed=allowed;active=false;trigger.reset();sources.clear();smoke.clear();
    warningSound.stop();eruptionSound.stop();eruptionSoundActive=false;grid.clear();hotMesh.clear();crustMesh.clear();smokeMesh.clear();
    lastUpdate=-1;nextScan=0;accumulator=0;
    std::cout<<"[VOLCANO] "<<(armed?"ON: build a broad stable mound":"OFF")<<std::endl;
}

void Volcano::rebuild(const ofRectangle& r)
{
    warningSound.stop();eruptionSound.stop();eruptionSoundActive=false;
    if(!demoMode)MagicSandTreeFire::contactMap().clear();MagicSandLavaCover::map().clear();belowResetSince=-1;
    volcanoSleeping=false;lastInteraction=-1;interactionHeights.clear();
    roi=r;
    float step=std::max(5.0f,std::max(roi.width,roi.height)/80.0f);
    grid.setup(std::max(2,static_cast<int>(std::ceil(roi.width/step))),
               std::max(2,static_cast<int>(std::ceil(roi.height/step))));
    blocked.assign(grid.width*grid.height,0);
    corners.assign((grid.width+1)*(grid.height+1),ofVec2f());
    trigger.reset();sources.clear();smoke.clear();hotMesh.clear();crustMesh.clear();smokeMesh.clear();
    accumulator=0;nextScan=0;nextMesh=0;rawFreshAt=-1000;sequence=0;
    nextTerrainScan=0;nextSmokeMesh=0;renderMapDirty=true;lavaMeshDirty=true;
    renderPositions.clear();renderUV.clear();lavaRenderSamples.clear();
}

ofVec2f Volcano::cellCenter(int i) const
{
    return ofVec2f(roi.x+(i%grid.width+.5f)*roi.width/grid.width,
                  roi.y+(i/grid.width+.5f)*roi.height/grid.height);
}

ofVec2f Volcano::project(float x,float y) const
{
    float gx=ofClamp((x-roi.x)*grid.width/roi.width,0.0f,static_cast<float>(grid.width));
    float gy=ofClamp((y-roi.y)*grid.height/roi.height,0.0f,static_cast<float>(grid.height));
    int ix=std::min(static_cast<int>(gx),grid.width-1),iy=std::min(static_cast<int>(gy),grid.height-1);
    float u=gx-ix,v=gy-iy;
    return corners[iy*(grid.width+1)+ix]*((1-u)*(1-v))+
           corners[iy*(grid.width+1)+ix+1]*(u*(1-v))+
           corners[(iy+1)*(grid.width+1)+ix]*((1-u)*v)+
           corners[(iy+1)*(grid.width+1)+ix+1]*(u*v);
}

void Volcano::scan(float now)
{
    uint64_t serial=0;
    if(projector->copyRainDepthSnapshot(raw,serial) && serial!=sequence) {
        sequence=serial;rawFreshAt=now;
    }
    const bool fresh=raw.isAllocated()&&now-rawFreshAt<.45f;
    // Retain a median sand reference for diagnostics. The adjustable hand
    // floor below is measured above the calibrated water level.
    std::vector<float> heights;
    if(fresh) for(int i=0;i<grid.width*grid.height;++i) {
        auto c=cellCenter(i);int x=static_cast<int>(c.x),y=static_cast<int>(c.y);
        if(x<0||y<0||x>=static_cast<int>(raw.getWidth())||y>=static_cast<int>(raw.getHeight()))continue;
        float d=raw[y*raw.getWidth()+x];
        float h=projector->rainElevationFromDepth(x,y,d);
        if(d>=450&&d<=4500&&std::isfinite(h)&&h>-700&&h<900)heights.push_back(h);
    }
    if(!heights.empty()) {
        auto middle=heights.begin()+heights.size()/2;
        std::nth_element(heights.begin(),middle,heights.end());
        sandReference=*middle;
    }
    handCeiling=MagicSandTriggers::values().handFloor;
    const bool refreshTerrain=now>=nextTerrainScan;
    if(refreshTerrain) {
        nextTerrainScan=now+((trigger.latched&&!trigger.erupting) ? 1.0f : (trigger.erupting ? .5f : .15f));
    }
    bool mappingChanged=false;
    resetMotionCells=0;
    if(interactionHeights.size()!=grid.ground.size())
        interactionHeights.assign(grid.ground.size(),std::numeric_limits<float>::quiet_NaN());
    raisedCells=0;
    const auto plane=projector->getBasePlaneEq();
    for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x) {
        int i=y*grid.width+x;auto c=cellCenter(i);
        int kx=static_cast<int>(c.x),ky=static_cast<int>(c.y);
        blocked[i]=!fresh;
        if(fresh) {
            if(kx<0||ky<0||kx>=static_cast<int>(raw.getWidth())||ky>=static_cast<int>(raw.getHeight()))blocked[i]=1;
            else {
                float d=raw[ky*raw.getWidth()+kx];
                float h=projector->rainElevationFromDepth(kx,ky,d);
                if(std::isfinite(h)&&d>=450&&d<=4500&&h>handCeiling)++raisedCells;
                blocked[i]=d<450||d>4500||!std::isfinite(h)||h>handCeiling;
                if(!blocked[i]) {
                    if(trigger.latched&&!trigger.erupting&&std::isfinite(interactionHeights[i])&&std::fabs(h-interactionHeights[i])>10)
                        ++resetMotionCells;
                    interactionHeights[i]=h;
                } else interactionHeights[i]=std::numeric_limits<float>::quiet_NaN();
            }
        }
        if(blocked[i]||!refreshTerrain)continue;
        float sx=roi.width/grid.width*.27f,sy=roi.height/grid.height*.27f;
        float dx[]={0,sx,-sx,0,0},dy[]={0,0,0,sy,-sy};
        float samples[5];int count=0;
        for(int q=0;q<5;++q) {
            auto world=projector->kinectCoordToWorldCoord(c.x+dx[q],c.y+dy[q]);
            float e=-(plane.x*world.x+plane.y*world.y+plane.z*world.z+plane.w);
            if(std::isfinite(e)&&world.z>100&&e>-700&&e<handCeiling)samples[count++]=e;
        }
        if(count>=3) {
            for(int a=1;a<count;++a){float v=samples[a];int b=a;while(b>0&&samples[b-1]>v){samples[b]=samples[b-1];--b;}samples[b]=v;}
            grid.ground[i]=samples[1];grid.valid[i]=1;
        } else blocked[i]=1;
    }
    if(refreshTerrain) for(int y=0;y<=grid.height;++y)for(int x=0;x<=grid.width;++x) {
        int near=std::min(y,grid.height-1)*grid.width+std::min(x,grid.width-1);
        if(blocked[near])continue;
        float px=ofClamp(roi.x+x*roi.width/grid.width,0.0f,static_cast<float>(raw.getWidth()-1));
        float py=ofClamp(roi.y+y*roi.height/grid.height,0.0f,static_cast<float>(raw.getHeight()-1));
        auto p=projector->kinectCoordToProjCoord(px,py);
        if(std::isfinite(p.x)&&std::isfinite(p.y)) {
            auto& previous=corners[y*(grid.width+1)+x];
            if((p-previous).length()>.25f) {previous=p;mappingChanged=true;}
        }
    }
    if(mappingChanged) {renderMapDirty=true;lavaMeshDirty=true;}
}

bool Volcano::evaluateMoundAt(int peak,float& prominence)
{
    prominence=0;moundBroad=moundLower=moundRim=0;
    moundReason="invalid summit";
    const int w=grid.width,h=grid.height;
    int radius=std::max(4,static_cast<int>(std::min(w,h)*.20f));
    if(peak<0||peak>=w*h||!grid.valid[peak]||blocked[peak])return false;
    int x=peak%w,y=peak/w;
    if(x<radius||y<radius||x>=w-radius||y>=h-radius)return false;
    float summit=grid.ground[peak];
    // Keep the original vent even if the sampled top moves by a cell.
    // Shape/relief must still hold around this same physical mound.
    for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx) {
        int j=yy*w+xx;
        if(grid.valid[j]&&!blocked[j]&&grid.ground[j]>=summit-35)++moundBroad;
    }
    float rim=0;
    for(int n=0;n<8;++n) {
        float a=n*6.283185307f/8;
        int xx=x+static_cast<int>(std::round(std::cos(a)*radius));
        int yy=y+static_cast<int>(std::round(std::sin(a)*radius));
        int j=yy*w+xx;
        if(!grid.valid[j]||blocked[j])continue;
        ++moundRim;rim+=grid.ground[j];
        if(summit-grid.ground[j]>=10)++moundLower;
    }
    prominence=moundRim ? summit-rim/moundRim : 0;
    bool peakAllowed=MagicSandTriggers::peakHeight(summit);
    bool qualifies=peakAllowed&&moundRim==8&&prominence>=20&&moundLower>=5&&moundBroad>=7;
    moundReason=!peakAllowed?"below selected peak height":moundRim<8?"rim obscured":moundBroad<7?"summit too narrow":
        prominence<20?"not enough surrounding drop":moundLower<5?"ridge, not a mound":"ready";
    return qualifies;
}

bool Volcano::findMound(int& peak,float& prominence,bool& lowered)
{
    peak=-1;prominence=0;lowered=false;
    moundBroad=moundLower=moundRim=0;moundReason="no valid summit";
    // Once holding/warning starts, never substitute an unrelated hill.
    // Re-evaluate the locked vent itself; another hill cannot reset the clock.
    if(trigger.vent>=0&&(trigger.stableSince>=0||trigger.warning||trigger.latched||trigger.erupting)) {
        peak=trigger.vent;
        bool good=evaluateMoundAt(peak,prominence);
        lowered=moundRim==8 && prominence<8;
        return good;
    }
    const int w=grid.width,h=grid.height;
    int radius=std::max(4,static_cast<int>(std::min(w,h)*.20f));
    float bestScore=-1e20f;bool found=false;
    int bestBroad=0,bestLower=0,bestRim=0;std::string bestReason=moundReason;
    for(int y=radius;y<h-radius;++y)for(int x=radius;x<w-radius;++x) {
        int i=y*w+x;if(!grid.valid[i]||blocked[i])continue;
        auto p=cellCenter(i);float nx=(p.x-roi.x)/roi.width,ny=(p.y-roi.y)/roi.height;
        if(ny<.23f&&nx>.22f&&nx<.78f)continue;
        // Former volcano-button location is now usable terrain.
        bool localPeak=true;
        for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx) {
            int j=yy*w+xx;
            if(grid.valid[j]&&!blocked[j]&&grid.ground[j]>grid.ground[i]+2)localPeak=false;
        }
        if(!localPeak)continue;
        float relief=0;bool qualifies=evaluateMoundAt(i,relief);
        // Among qualified mounds choose the higher summit, not whichever
        // distant basin gains a fraction of a millimetre of relief noise.
        float score=(qualifies?100000.0f:0)+(moundRim==8?1000.0f:0)+
                    (qualifies?grid.ground[i]:relief);
        if(score>bestScore) {
            bestScore=score;peak=i;prominence=relief;found=qualifies;
            bestBroad=moundBroad;bestLower=moundLower;bestRim=moundRim;bestReason=moundReason;
        }
    }
    moundBroad=bestBroad;moundLower=bestLower;moundRim=bestRim;moundReason=bestReason;
    lowered=peak>=0&&moundRim==8&&prominence<8;
    return found;
}

void Volcano::makeSources()
{
    sources.clear();int w=grid.width;
    int cx=trigger.vent%w,cy=trigger.vent/w;
    for(int y=cy-2;y<=cy+2;++y)for(int x=cx-2;x<=cx+2;++x) {
        if(x<0||y<0||x>=w||y>=grid.height)continue;
        int i=y*w+x;
        if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=4&&grid.valid[i])sources.push_back(i);
    }
}

void Volcano::update(const std::shared_ptr<KinectProjector>& k,bool normalMode)
{
    float now=ofGetElapsedTimef();
    const auto& controls=MagicSandTriggers::values();
    if(lastPeakTarget!=controls.volcanoPeak||lastHandFloor!=controls.handFloor) {
        lastPeakTarget=controls.volcanoPeak;lastHandFloor=controls.handFloor;nextScan=0;nextTerrainScan=0;
        // Recheck a warning from scratch; preserve an existing eruption/latch.
        if(!trigger.erupting&&!trigger.latched) {
            trigger.reset();warningSound.stop();
        }
    }
    if(!armed||!normalMode||!k||!k->isImageStabilized()) {
        if(active){if(!demoMode)MagicSandTreeFire::contactMap().clear();MagicSandLavaCover::map().clear();belowResetSince=-1;
    volcanoSleeping=false;lastInteraction=-1;interactionHeights.clear();trigger.reset();warningSound.stop();eruptionSound.stop();eruptionSoundActive=false;smoke.clear();grid.clear();hotMesh.clear();crustMesh.clear();smokeMesh.clear();}
        MagicSandTriggers::observedPeak()=std::numeric_limits<float>::quiet_NaN();
        active=false;lastUpdate=now;accumulator=0;return;
    }
    projector=k;auto r=k->getKinectROI();if(r.width<30||r.height<30){if(!demoMode)MagicSandTreeFire::contactMap().clear();MagicSandLavaCover::map().clear();belowResetSince=-1;
    volcanoSleeping=false;lastInteraction=-1;interactionHeights.clear();return;}
    if(!active||r!=roi)rebuild(r);
    active=true;
    if(!assetsTried) {
        assetsTried=true;
        lavaLoaded=lavaImage.load("volcano/lava.jpg");
        smokeLoaded=smokeImage.load("volcano/whitesmoke.png");
        soundLoaded=warningSound.load("volcano/warning.wav");
        eruptionSoundLoaded=eruptionSound.load("volcano/eruption.wav");
        eruptionSound.setVolume(audioEnabled?.75f:0);eruptionSound.setLoop(false);eruptionSound.setMultiPlay(false);
        warningSound.setVolume(audioEnabled?.8f:0);warningSound.setLoop(false);warningSound.setMultiPlay(false);
        std::cout<<"[VOLCANO] lava="<<lavaLoaded<<" smoke="<<smokeLoaded<<" warning="<<soundLoaded<<" eruptionSound="<<eruptionSoundLoaded<<std::endl;
    }
    ofSoundUpdate();
    float dt=lastUpdate>=0 ? ofClamp(now-lastUpdate,0.0f,.1f) : 0;lastUpdate=now;
    bool resetSampleReady=false;
    if(now>=nextScan) {
        nextScan=now+((trigger.latched&&!trigger.erupting) ? .20f : .15f);scan(now);
        resetSampleReady=true;
        int peak=-1;float prominence=0;bool lowered=false;
        bool mound=findMound(peak,prominence,lowered);
        MagicSandTriggers::observedPeak()=peak>=0?grid.ground[peak]:std::numeric_limits<float>::quiet_NaN();
        bool clear=now-rawFreshAt<.45f && raisedCells<4;
        static float nextVolcanoCheck=0;
        if(now>=nextVolcanoCheck) {
            nextVolcanoCheck=now+2;
            std::cout<<"[VOLCANO CHECK V3] summit="<<(peak>=0?grid.ground[peak]:-999)
                     <<"mm target="<<controls.volcanoPeak<<"mm relief="<<prominence<<"mm (need 20)"
                     <<" rim="<<moundRim<<"/8 lowerSides="<<moundLower<<"/5 broad="<<moundBroad<<"/7"
                     <<" shapeOK="<<mound<<" reason="<<moundReason
                     <<" handClear="<<clear<<" raised="<<raisedCells
                     <<" sand="<<sandReference<<" handCutoff="<<handCeiling
                     <<" rawAge="<<(now-rawFreshAt)
                     <<" steady="<<(trigger.stableSince>=0?now-trigger.stableSince:0)
                     <<" warning="<<trigger.warning<<" erupting="<<trigger.erupting
                     <<" latched="<<trigger.latched<<" lavaTotal="<<grid.total()
                     <<" sourceCells="<<sources.size()<<std::endl;
        }

        // Compare local relief, so uniform depth/base-plane drift cannot
        // repeatedly cancel a stable mound's countdown.
        if(!demoMode && peak>=0 && trigger.stableSince>=0 &&
           std::abs(prominence-peakHoldRelief)>8.0f) mound=false;
        if(peak>=0 && trigger.stableSince<0)peakHoldRelief=prominence;
        int event=trigger.update(now,true,demoMode||clear,demoMode||mound,false,peak);
        if(event==1) {
            if(soundLoaded){warningSound.stop();warningSound.setPosition(0);warningSound.play();}
            windAngle=ofRandom(6.283185307f);nextSmoke=now;
            std::cout<<"[VOLCANO] WARNING: stable summit, prominence "<<prominence<<" mm; eruption in 5 seconds"<<std::endl;
        } else if(event==2) {
            eruptionResetHeight=controls.volcanoPeak;belowResetSince=-1;lastInteraction=-1;volcanoSleeping=false;
            warningSound.stop();
            if(eruptionSoundLoaded) {
                eruptionSound.stop();eruptionSound.setPosition(0);eruptionSound.play();
                eruptionSoundActive=true;ofSoundUpdate();
            }
            std::cout<<"[VOLCANO AUDIO] eruption WAV "
                     <<(eruptionSoundLoaded ? "started with lava" : "UNAVAILABLE")<<std::endl;
            makeSources();std::cout<<"[VOLCANO] ERUPTION: 30 seconds; "<<sources.size()<<" lava source cells; wind fixed for this eruption"<<std::endl;
        } else if(event==-1) {
            warningSound.stop();std::cout<<"[VOLCANO] warning cancelled: handClear="<<clear<<" moundOK="<<mound<<" relief="<<prominence<<" reason="<<moundReason<<""<<std::endl;
        }
    }
    // VOLCANO_RESET_V2. Confirm only on new camera checks. A skipped render
    // frame never invalidates a previously accepted height sample.
    if(!demoMode&&trigger.latched&&!trigger.erupting) {
        if(resetSampleReady) {
            const bool fresh=now-rawFreshAt<.45f;
            const bool clear=fresh&&raisedCells<4;
            const bool interacted=fresh&&(raisedCells>=4||resetMotionCells>=std::max(6,int(grid.ground.size()/100)));
            if(lastInteraction<0)lastInteraction=now;
            const bool wake=volcanoSleeping&&interacted;
            if(interacted)lastInteraction=now;
            if(!volcanoSleeping&&now-lastInteraction>=60) {
                volcanoSleeping=true;
                std::cout<<"[VOLCANO IDLE] unattended mound: dormant until hand/sand movement or peak lowering"<<std::endl;
            }
            bool lowered=false;
            float originalPeak=-1000;
            if(clear&&trigger.vent>=0&&trigger.vent<int(grid.ground.size())) {
                if(eruptionResetHeight>0) {
                    int cx=trigger.vent%grid.width,cy=trigger.vent/grid.width;
                    lowered=true;
                    for(int yy=cy-1;yy<=cy+1;++yy)for(int xx=cx-1;xx<=cx+1;++xx) {
                        if(xx<0||yy<0||xx>=grid.width||yy>=grid.height){lowered=false;continue;}
                        auto c=cellCenter(yy*grid.width+xx);int x=int(c.x),y=int(c.y);
                        if(x<0||y<0||x>=int(raw.getWidth())||y>=int(raw.getHeight())){lowered=false;continue;}
                        float depth=raw[y*raw.getWidth()+x];
                        float height=projector->rainElevationFromDepth(x,y,depth);
                        if(depth<450||depth>4500||!std::isfinite(height)||height>handCeiling){lowered=false;continue;}
                        originalPeak=std::max(originalPeak,height);
                        if(height>=eruptionResetHeight-3)lowered=false;
                    }
                } else {
                    float relief=0;evaluateMoundAt(trigger.vent,relief);
                    lowered=moundRim==8&&relief<8;
                    originalPeak=grid.ground[trigger.vent];
                }
            }
            if(!lowered)belowResetSince=-1;
            else if(belowResetSince<0)belowResetSince=now;
            static float nextResetReport=0;
            if(now>=nextResetReport) {
                nextResetReport=now+2;
                std::cout<<"[VOLCANO RESET CHECK] originalPeak="<<originalPeak
                         <<" threshold="<<eruptionResetHeight<<" below="<<lowered
                         <<" handClear="<<clear<<" sleeping="<<volcanoSleeping
                         <<" motionCells="<<resetMotionCells
                         <<" confirmed="<<(belowResetSince<0?0:now-belowResetSince)<<std::endl;
            }
            if(wake||(lowered&&belowResetSince>=0&&now-belowResetSince>=1)) {
                trigger.reset();grid.clear();sources.clear();smoke.clear();
                hotMesh.clear();crustMesh.clear();smokeMesh.clear();
                warningSound.stop();eruptionSound.stop();eruptionSoundActive=false;
                if(!demoMode)MagicSandTreeFire::contactMap().clear();MagicSandLavaCover::map().clear();
                belowResetSince=-1;accumulator=0;nextScan=now+.2f;nextTerrainScan=0;lavaMeshDirty=false;
                volcanoSleeping=false;lastInteraction=-1;interactionHeights.clear();
                std::cout<<(wake?"[VOLCANO RESET] new visitor movement woke dormant volcano; hand-clear and countdown still required":
                                 "[VOLCANO RESET] eruption finished and original peak lowered; ready for a new mound")<<std::endl;
            }
        }
    } else belowResetSince=-1;
    if(eruptionSoundActive && !trigger.erupting) {
        eruptionSound.stop();eruptionSoundActive=false;
    }
    accumulator=std::min(.1f,accumulator+dt);
    // Only mobile lava needs flow/heat updates. Solid crust stays cached.
    if(trigger.erupting||grid.mobileTotal()>.000001f) {
        while(accumulator>=.025f) {
            if(trigger.erupting&&!sources.empty())
                for(int i:sources)grid.add(i,100.0f*.025f/sources.size());
            grid.step(.025f);accumulator-=.025f;lavaMeshDirty=true;
        }
    } else accumulator=0;
    if((trigger.warning||trigger.erupting)&&trigger.vent>=0&&now>=nextSmoke) {
        nextSmoke=now+(trigger.erupting ? .20f : .55f);
        if(smoke.size()<64)smoke.push_back({cellCenter(trigger.vent),now,ofRandom(8,14),ofRandom(360)});
    }
    smoke.erase(std::remove_if(smoke.begin(),smoke.end(),[&](const Puff& p){return now-p.born>9;}),smoke.end());
    if(lavaMeshDirty&&now>=nextMesh) {
        nextMesh=now+(trigger.erupting ? .25f : .5f);
        if(!trigger.erupting)settleColdLava();
        buildMeshes();lavaMeshDirty=false;
    }
    if(now>=nextSmokeMesh) {nextSmokeMesh=now+.125f;
        if(!smoke.empty()||smokeMesh.getNumVertices()>0)buildSmokeMesh(now);
    }
}

// Once lava is below the render's glow threshold, preserve its volume as
// stationary crust. This ends work on the invisible cold liquid tail.
void Volcano::settleColdLava()
{
    for(size_t i=0;i<grid.lava.size();++i) {
        if(grid.lava[i]>0&&grid.heat(int(i))<=.15f) {
            grid.crust[i]+=grid.lava[i];grid.lava[i]=0;grid.energy[i]=0;
        }
    }
}

void Volcano::buildMeshes()
{
    if(!demoMode)MagicSandTreeFire::contactMap().publish(roi.x,roi.y,roi.width,roi.height,
        grid.width,grid.height,grid.lava,grid.energy,grid.valid,ofGetElapsedTimef());
    MagicSandLavaCover::map().publish(roi.x,roi.y,roi.width,roi.height,
        grid.width,grid.height,grid.lava,grid.crust,grid.valid);
    hotMesh.clear();crustMesh.clear();
    hotMesh.setMode(OF_PRIMITIVE_TRIANGLES);crustMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    // Draw at twice the simulation resolution. Interpolate conserved fluid
    // depth/heat and crust for rendering only; the working solver is untouched.
    const int rw=grid.width*2,rh=grid.height*2;
    auto& vertices=lavaRenderSamples;vertices.clear();
    vertices.reserve((rw+1)*(rh+1));
    if(renderMapDirty||renderPositions.size()!=static_cast<size_t>((rw+1)*(rh+1))) {
        renderPositions.clear();renderPositions.reserve((rw+1)*(rh+1));
        for(int y=0;y<=rh;++y)for(int x=0;x<=rw;++x)
            renderPositions.push_back(project(roi.x+x*.5f*roi.width/grid.width,
                                              roi.y+y*.5f*roi.height/grid.height));
        renderMapDirty=false;
    }
    if(lavaLoaded&&renderUV.size()!=625) {
        renderUV.clear();renderUV.reserve(625);
        for(int y=0;y<=24;++y)for(int x=0;x<=24;++x)
            renderUV.push_back(lavaImage.getTexture().getCoordFromPercent(x/24.0f,y/24.0f));
    }
    for(int y=0;y<=rh;++y)for(int x=0;x<=rw;++x) {
        float fx=x*.5f,fy=y*.5f;
        float gx=fx-.5f,gy=fy-.5f;
        int ix=static_cast<int>(std::floor(gx)),iy=static_cast<int>(std::floor(gy));
        float tx=gx-ix,ty=gy-iy;
        float liquid=0,solid=0,energy=0;
        for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {
            int xx=std::max(0,std::min(grid.width-1,ix+dx));
            int yy=std::max(0,std::min(grid.height-1,iy+dy));
            int i=yy*grid.width+xx;
            if(!grid.valid[i])continue;
            float weight=(dx?tx:1-tx)*(dy?ty:1-ty);
            liquid+=grid.lava[i]*weight;solid+=grid.crust[i]*weight;
            energy+=grid.energy[i]*weight;
        }
        float volume=liquid+solid;
        float coverage=ofClamp(volume/.65f,0.0f,1.0f);
        // Smooth alpha gives a soft advancing edge instead of filled squares.
        coverage=coverage*coverage*(3-2*coverage);
        float temperature=liquid>.00001f ? ofClamp(energy/liquid,0.0f,1.0f) : 0;
        float heat=ofClamp((temperature-.15f)/.65f,0.0f,1.0f);
        float alpha=coverage*ofClamp(liquid/std::max(.01f,volume)*heat*1.2f,0.0f,1.0f);
        vertices.push_back({renderPositions[y*(rw+1)+x],ofFloatColor(.065f,.035f,.025f,coverage*.92f),
            ofFloatColor(1.0f,.18f+.82f*heat,.03f+.97f*heat,alpha),volume});
    }
    const int triangles[]={0,1,2,0,2,3};
    for(int y=0;y<rh;++y)for(int x=0;x<rw;++x) {
        int indices[]={y*(rw+1)+x,y*(rw+1)+x+1,(y+1)*(rw+1)+x+1,(y+1)*(rw+1)+x};
        float largest=0,maxHot=0;
        for(int j:indices){largest=std::max(largest,vertices[j].volume);maxHot=std::max(maxHot,vertices[j].hot.a);}
        if(largest<.015f)continue;
        // Preserve the original physical texture scale: 12 simulation cells.
        int u=x%24,v=y%24;
        int uvIndex[]={v*25+u,v*25+u+1,(v+1)*25+u+1,(v+1)*25+u};
        for(int n:triangles) {
            const auto& sample=vertices[indices[n]];
            crustMesh.addVertex(ofVec3f(sample.position.x,sample.position.y,0));
            crustMesh.addColor(sample.base);
            if(maxHot>.001f) {
                hotMesh.addVertex(ofVec3f(sample.position.x,sample.position.y,0));
                hotMesh.addColor(sample.hot);
                if(lavaLoaded)hotMesh.addTexCoord(renderUV[uvIndex[n]]);
            }
        }
    }
}

void Volcano::draw()
{
    if(!armed||!active)return;
    float now=ofGetElapsedTimef();ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);
    crustMesh.draw();
    if(lavaLoaded)lavaSurfaceMotion.draw(hotMesh,lavaImage,now); // LAVA_DISTORTION_V30
    else hotMesh.draw();
    if((trigger.warning||trigger.erupting)&&trigger.vent>=0) {
        auto c=cellCenter(trigger.vent);float radius=(roi.width/grid.width)*2.4f;
        ofNoFill();ofSetLineWidth(2.5f);
        ofSetColor(255,trigger.warning ? 60 : 160,15,static_cast<int>(150+80*std::sin(now*7)));
        for(int n=0;n<40;++n) {
            float a=n*6.283185307f/40,b=(n+1)*6.283185307f/40;
            auto p=project(c.x+std::cos(a)*radius,c.y+std::sin(a)*radius);
            auto q=project(c.x+std::cos(b)*radius,c.y+std::sin(b)*radius);
            ofDrawLine(p.x,p.y,q.x,q.y);
        }
        if(trigger.warning&&!demoMode) { // BONE_BUTTON_V8: no projected demo countdown text
            auto p=project(c.x,c.y);
            int seconds=std::max(1,static_cast<int>(std::ceil(5-(now-trigger.warningAt))));
            ofSetColor(255,220,120);ofDrawBitmapString("VOLCANO: "+ofToString(seconds),p.x+12,p.y-12);
        }
    }
    ofFill();
    if(smokeLoaded) {
        ofSetColor(255);smokeImage.getTexture().bind();smokeMesh.draw();smokeImage.getTexture().unbind();
    }
    ofPopStyle();
}

void Volcano::buildSmokeMesh(float now)
{
    smokeMesh.clear();smokeMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    if(!smokeLoaded||smoke.empty())return;
    const int triangles[]={0,1,2,0,2,3};
    ofVec2f uv[]={smokeImage.getTexture().getCoordFromPercent(0,0),
        smokeImage.getTexture().getCoordFromPercent(1,0),
        smokeImage.getTexture().getCoordFromPercent(1,1),
        smokeImage.getTexture().getCoordFromPercent(0,1)};
    for(const auto& puff:smoke) {
        float age=now-puff.born,t=age/9;
        float speed=roi.width*.025f;
        float x=puff.position.x+std::cos(windAngle)*speed*age;
        float y=puff.position.y+std::sin(windAngle)*speed*age;
        if(x<roi.x||x>roi.x+roi.width||y<roi.y||y>roi.y+roi.height)continue;
        auto center=project(x,y);
        float size=puff.size*(1+age*.36f);
        auto edge=project(std::min(roi.x+roi.width,x+size),y);
        float diameter=std::max(8.0f,(edge-center).length()*2);
        float alpha=110*std::min(1.0f,age*3)*(1-t)*(1-t);
        float angle=(puff.rotation+age*4)*.01745329252f;
        float ca=std::cos(angle),sa=std::sin(angle),half=diameter*.5f;
        ofVec2f offsets[]={ofVec2f(-half,-half),ofVec2f(half,-half),
            ofVec2f(half,half),ofVec2f(-half,half)};
        ofFloatColor color(190/255.0f,180/255.0f,170/255.0f,static_cast<int>(alpha)/255.0f);
        for(int n:triangles) {
            const auto& p=offsets[n];
            smokeMesh.addVertex(ofVec3f(center.x+p.x*ca-p.y*sa,center.y+p.x*sa+p.y*ca,0));
            smokeMesh.addColor(color);smokeMesh.addTexCoord(uv[n]);
        }
    }
}

bool Volcano::beginDemo(const std::shared_ptr<KinectProjector>& k) {
    demoMode=true;setEnabled(true);update(k,true);
    if(!active)return false;
    int peak=-1;
    for(int i=0;i<int(grid.ground.size());++i)
        if(grid.valid[i]&&!blocked[i]&&(peak<0||grid.ground[i]>grid.ground[peak]))peak=i;
    if(peak<0)return false;
    trigger.reset();trigger.vent=peak;trigger.stableSince=ofGetElapsedTimef()-3.1f;
    nextScan=0;return true;
}
void Volcano::endDemo() {setEnabled(false);demoMode=false;}
