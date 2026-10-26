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
/* Thunder sound effect by Friendly Free Sounds.
 * https://www.youtube.com/watch?v=xK_m77VZYnc
 * Original recorded WAV retained with channel credit for TopoPlay.
 * Operator-supplied creator permission and project-use confirmation:
 * docs/assets/THUNDER-PERMISSION.md. This is not generated audio.
 */
#include "RainRunoff.h"
// RAIN_CLOUD_REFINE_V9: immediate visitor renewal, color-only water, stronger neutral overhead cloud.
#include <cmath>
#include <algorithm>

void RainRunoff::rebuild(const ofRectangle& area)
{
    roi=area;
    interactiveWater.reset(); // WATER_RIPPLES_V60: ROI changes cannot retain old waves.
    const float step=std::max(4.0f,std::max(roi.width,roi.height)/100.0f);
    grid.setup(std::max(2,static_cast<int>(std::ceil(roi.width/step))),
               std::max(2,static_cast<int>(std::ceil(roi.height/step))));
    // WATER_FLOW_V25: physical cell spacing from calibrated camera rays on
    // the tilted base plane, not camera-pixel spacing or the projected image.
    auto transposed=projector->getTransposedKinectWorldMatrix();
    const auto cameraMatrix=transposed.getTransposedOf(transposed);
    const auto plane=projector->getBasePlaneEq();
    auto planePoint=[&](float x,float y,ofVec3f& point){
        const ofVec4f ray=cameraMatrix*ofVec4f(x,y,1,1);
        const float denominator=plane.x*ray.x+plane.y*ray.y+plane.z*ray.z;
        if(!std::isfinite(denominator)||std::abs(denominator)<1.e-8f)return false;
        const float depth=-plane.w/denominator;
        if(!std::isfinite(depth)||depth<450||depth>4500)return false;
        point=ofVec3f(ray.x*depth,ray.y*depth,ray.z*depth);
        return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
    };
    float cellXSum=0,cellYSum=0;int cellXCount=0,cellYCount=0;
    for(float t:{.25f,.5f,.75f}){
        const float x=roi.x+roi.width*t,y=roi.y+roi.height*t;
        ofVec3f a,b;
        if(planePoint(x,y,a)&&planePoint(x+roi.width/grid.width,y,b)){
            float d=(b-a).length();if(d>=2&&d<=100){cellXSum+=d;++cellXCount;}}
        if(planePoint(x,y,a)&&planePoint(x,y+roi.height/grid.height,b)){
            float d=(b-a).length();if(d>=2&&d<=100){cellYSum+=d;++cellYCount;}}
    }
    grid.setCellSize(cellXCount?cellXSum/cellXCount:12.f,cellYCount?cellYSum/cellYCount:12.f);
    std::cout<<"[WATER FLOW V25] cellMm="<<grid.cellWidthMm()<<","<<grid.cellHeightMm()
        <<" metric="<<(cellXCount&&cellYCount?"calibrated":"12mm_fallback")
        <<" model=surge-v66 local-inertial+slow-film; bounded CFL substeps"<<std::endl;

    surfaceLook.setup(grid.width,grid.height); // RAIN_SATURATION_V26
    const int n=grid.width*grid.height;
    terrainCandidate.assign(n,0);terrainCandidateAt.assign(n,-1);nextTerrainReport=0;
    hand.assign(n,0);rain.assign(n,0);visited.assign(n,0);
    rainPatch.assign(n,0);patchCenters.clear();cloudMesh.clear();patchChosen=false;
    cloud.reset();nextCloudMesh=0;visitor=false;capturing=false;cloudCommitted=false;
    holdAudioStarted=false;wavStartedAt=-1000;nextWavFlash=0;
    corners.assign((grid.width+1)*(grid.height+1),ofVec2f(0,0));
    heldSince=-1;lastHandTime=-1000;raining=false;
    handLatched=false;stormPending=false;stormStart=-1000;stormEnd=-1000;
    stormFootprint.assign(n,0);stormAdded=0;stormBudget=0;activeSources=0;
    handClearSince=-1;lastOcclusionTime=-1000;stableProbes=0;nextStableProbe=0;
    previousGround=grid.ground;
    accumulator=0;nextTerrain=0;nextScan=0;nextMesh=0;
    waterMesh.clear();poolMesh.clear(); // BLUE_RAIN_POOLS_V27
    std::cout<<"[RAIN] grid "<<grid.width<<"x"<<grid.height
             <<"; hold a raised hand at the small moving cloud for FIVE seconds"<<std::endl;
}

void RainRunoff::update(const std::shared_ptr<KinectProjector>& k,bool active)
{
    const float now=ofGetElapsedTimef();
    float handFloor=MagicSandTriggers::values().handFloor;
    if(handFloor!=lastHandFloor) {
        lastHandFloor=handFloor;
        // Do not complete a gesture or pending rescan using an old height band.
        if(capturing||stormPending) {
            stormPending=false;heldSince=-1;lastHandTime=-1000;handLatched=false;
            handClearSince=-1;stableProbes=0;stormStart=-1000;stormEnd=-1000;
            flashUntil=-1000;
            if(cloud.active)releaseVisitor(now);
        }
    }
    if(soundTried) ofSoundUpdate();
    if(!active || !k || !k->isImageStabilized()) {
        if(enabled) {
            grid.clear();
            waterMesh.clear();
            std::fill(hand.begin(),hand.end(),0);
            std::fill(rain.begin(),rain.end(),0);
            thunder.stop();heldSince=-1;raining=false;
            handLatched=false;stormPending=false;stormEnd=-1000;flashUntil=-1000;
            handClearSince=-1;stableProbes=0;
            holdAudioStarted=false;wavStartedAt=-1000;nextWavFlash=0;cloudMesh.clear();cloud.reset();visitor=false;capturing=false;cloudCommitted=false;
        }
        interactiveWater.reset();
        enabled=false;lastUpdate=now;accumulator=0;return;
    }
    projector=k;
    const ofRectangle current=k->getKinectROI();
    if(current.width<10 || current.height<10) return;
    if(!enabled || current!=roi) rebuild(current);
    enabled=true;
    if(!soundTried) {
        soundTried=true;
        soundLoaded=thunder.load("rain/thunder.wav");
        std::cout<<"[RAIN] thunder audio loaded: "<<(soundLoaded ? "YES" : "NO")<<std::endl;
        thunder.setVolume(audioEnabled?.85f:0);thunder.setLoop(false);thunder.setMultiPlay(false);
    }
    if(!cloudTextureTried) {
        cloudTextureTried=true;
        cloudTextureLoaded=cloudSmoke.load("volcano/whitesmoke.png");
        std::cout<<"[STORM CLOUD] volcano smoke texture "
                 <<(cloudTextureLoaded?"loaded":"missing; soft mesh fallback")<<std::endl;
    }
    float dt=lastUpdate>=0 ? ofClamp(now-lastUpdate,0.0f,.1f) : 0;
    lastUpdate=now;
    if(rainArmed&&!cloud.active&&!stormPending&&!raining&&now>=stormEnd)makeVisitor(now);
    // Keep the visitor current before testing a hand against its position.
    updateCloud(now);
    if(rainArmed && now>=nextScan) {nextScan=now+(visitor ? .20f : .10f);if(demoMode)advanceDemo(now);else detectHand(now);}
    updateWavFlashes(now);
    updateCloud(now);
    updateStorm(now);
    const bool wet=grid.hasDynamics();
    if(now>=nextTerrain) {
        nextTerrain=now+((raining||wet||heldSince>=0) ? .25f : 1.0f);
        sampleTerrain();
    }
    // SURGE_WATER_V66: seed a level regular-mode sea once; retain its wave
    // storage/momentum thereafter. Demo pools stay isolated from regular sea.
    const auto& waterProfile=MagicSandAnimalTerrain::profile();
    grid.configureSea(!demoMode&&MagicSandWaterRipples::enabled(),
        waterProfile.enabled?waterProfile.levels[7]:0.f);
    // WAVE_RENDER_V71: reuse the newest CPU depth snapshot every app update,
    // rather than waiting for the slower cloud gesture/terrain scan schedule.
    uint64_t waveSequence=0;
    if(projector->copyRainDepthSnapshot(raw,waveSequence)&&waveSequence!=rawSequence){rawSequence=waveSequence;rawFreshTime=now;}
    // RAIN_SATURATION_V26: surface appearance is independent of flux pulses.
    surfaceLook.update(dt,grid.displayWater(),grid.valid,rain,raining?grid.rainRate:0.f);
    interactiveWater.update(projector,roi,grid,surfaceLook,corners,raw,rawSequence,
        now-rawFreshTime,now,raining,rain,!demoMode); // WATER_RIPPLES_V60
    if(!grid.hasDynamics() && !raining) {
        accumulator=0;
        if(surfaceLook.visible()){
            if(now>=nextMesh){nextMesh=now+.10f;buildMeshes(now);}
        }else waterMesh.clear();
        return;
    }
    accumulator=std::min(.1f,accumulator+dt);
    while(accumulator>=.025f) {
        if(raining && activeSources>0) {
            const float remaining=std::max(0.0f,stormBudget-stormAdded);
            grid.rainRate=std::min(grid.rainRate,remaining/(activeSources*.025f));
            stormAdded+=grid.rainRate*activeSources*.025f;
        }
        grid.step(.025f,rain);
        accumulator-=.025f;
    }
    if(now>=nextMesh) {nextMesh=now+.10f;buildMeshes(now);}
}

void RainRunoff::detectHand(float now)
{
    uint64_t sequence=0;
    if(projector->copyRainDepthSnapshot(raw,sequence) && sequence!=rawSequence) {
        rawSequence=sequence;rawFreshTime=now;
    }
    std::fill(hand.begin(),hand.end(),0);
    if(raw.isAllocated() && now-rawFreshTime<.45f) {
        for(int y=0;y<grid.height;++y) for(int x=0;x<grid.width;++x) {
            const int kx=static_cast<int>(roi.x+(x+.5f)*roi.width/grid.width);
            const int ky=static_cast<int>(roi.y+(y+.5f)*roi.height/grid.height);
            if(kx<0||ky<0||kx>=static_cast<int>(raw.getWidth())||ky>=static_cast<int>(raw.getHeight())) continue;
            const float depth=raw[ky*raw.getWidth()+kx];
            if(depth<450 || depth>4500) continue;
            const float h=projector->rainElevationFromDepth(kx,ky,depth);
            if(MagicSandTriggers::handHeight(h))
                hand[y*grid.width+x]=1;
        }
    }
    // Reject isolated depth noise: retain connected hand-sized blobs.
    std::fill(visited.begin(),visited.end(),0);
    std::vector<int> stack,component;
    std::vector<unsigned char> accepted(hand.size(),0);
    int count=0;
    for(int seed=0;seed<static_cast<int>(hand.size());++seed) {
        if(!hand[seed]||visited[seed]) continue;
        stack.clear();component.clear();stack.push_back(seed);visited[seed]=1;
        while(!stack.empty()) {
            const int i=stack.back();stack.pop_back();component.push_back(i);
            const int x=i%grid.width,y=i/grid.width;
            const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
            for(int d=0;d<4;++d) {
                const int xx=x+dx[d],yy=y+dy[d];
                if(xx<0||yy<0||xx>=grid.width||yy>=grid.height) continue;
                const int j=yy*grid.width+xx;
                if(hand[j]&&!visited[j]) {visited[j]=1;stack.push_back(j);}
            }
        }
        if(component.size()>=4 && component.size()<hand.size()/3) {
            for(int i:component) accepted[i]=1;
            count+=component.size();
        }
    }
    hand.swap(accepted);
    // All confirmed raised hands still block the terrain-rescan phase.
    if(count>=4)lastOcclusionTime=now;
    // Trigger only at the small cloud location, never elsewhere on the arm.
    int nearby=0;
    if(cloud.active&&(visitor||capturing)&&!stormPending&&!raining) {
        float reach=std::max(visitorRadius*1.25f,roi.width/grid.width*1.5f);
        for(int i=0;i<int(hand.size());++i)if(hand[i]) {
            float dx=roi.x+(i%grid.width+.5f)*roi.width/grid.width-cloud.x;
            float dy=roi.y+(i/grid.width+.5f)*roi.height/grid.height-cloud.y;
            if(dx*dx+dy*dy<=reach*reach)++nearby;
        }
    }
    if(nearby>=4) {
        lastHandTime=now;
        if(heldSince<0) {
            heldSince=now;
            if(visitor) {
                visitor=false;capturing=true;cloudCommitted=false;
                cloud.originX=cloud.x;cloud.originY=cloud.y;cloud.driftAt=-1;
                nextCloudMesh=0;
            }
        }
    } else if(now-lastHandTime>.30f) {
        heldSince=-1;handLatched=false;
        if(capturing&&!cloudCommitted&&!stormPending)releaseVisitor(now);
        if(holdAudioStarted&&!stormPending&&now>=stormEnd) {
            thunder.stop();holdAudioStarted=false;wavStartedAt=-1000;nextWavFlash=0;
            patchChosen=false;patchCenters.clear();
        }
    }
    const bool heldFourSeconds=heldSince>=0 && now-heldSince>=4.0f && now-lastHandTime<.25f;
    if(heldFourSeconds && !holdAudioStarted && !stormPending && now>=stormEnd) {
        patchChosen=false;makeRainPatch();
        holdAudioStarted=true;wavStartedAt=now;nextWavFlash=0;
        if(soundLoaded) {
            thunder.stop();thunder.setPosition(0);thunder.play();ofSoundUpdate();
        }
        std::cout<<"[RAIN] FOUR-SECOND hold: WAV clock started; first flash at five seconds"<<std::endl;
    }
    const bool heldFiveSeconds=heldSince>=0 && now-heldSince>=5.0f && now-lastHandTime<.25f;
    if(heldFiveSeconds && !handLatched) {
        handLatched=true;
        // A held hand triggers one storm, rather than pouring indefinitely.
        if(!stormPending && now>=stormEnd) {
            if(!patchChosen) makeRainPatch();
            cloudCommitted=true;cloud.radius=patchRadius;nextCloudMesh=0;
            updateCloud(now);
            stormFootprint=rainPatch;
            stormPending=true;
            // First allow the flash to finish and wait one full second.
            // Then require hand removal and stable raw terrain statistics.
            rescanAt=now+1.40f;
            handClearSince=-1;stableProbes=0;nextStableProbe=0;
            flashUntil=now+.40f;
            lastStorm=now;
            std::fill(rain.begin(),rain.end(),0);
            // Audio has already started at the four-second hold mark.
            std::cout<<"[RAIN] FIVE-SECOND trigger: first whole-box flash; visitor grew into a storm; "
                     <<"waiting for the raised hand to leave"<<std::endl;
        }
    }
}

void RainRunoff::updateStorm(float now)
{
    std::fill(rain.begin(),rain.end(),0);
    activeSources=0;
    if(!rainArmed) {raining=false;grid.rainRate=0;return;}
    if(stormPending && now>=rescanAt) {
        const bool fresh=now-rawFreshTime<.35f;
        const bool clear=fresh && now-lastOcclusionTime>.35f;
        if(!clear) {
            handClearSince=-1;stableProbes=0;
            grid.rainRate=0;raining=false;
            return;
        }
        if(handClearSince<0) {
            handClearSince=now;nextStableProbe=now+1.25f;stableProbes=0;
            std::cout<<"[RAIN] hands clear; allowing terrain to settle"<<std::endl;
        }
        if(now<nextStableProbe) {grid.rainRate=0;raining=false;return;}
        nextStableProbe=now+.25f;
        previousGround=grid.ground;
        sampleTerrain(true);
        nextTerrain=now+.25f;
        // RAIN_START_V4: raw Kinect samples have isolated noisy pixels.
        // Compare the footprint as a whole instead of vetoing on one maximum.
        std::vector<float> changes;changes.reserve(stormFootprint.size());
        for(size_t i=0;i<stormFootprint.size();++i) {
            if(stormFootprint[i]&&grid.valid[i]) {
                const float change=std::fabs(grid.ground[i]-previousGround[i]);
                if(std::isfinite(change))changes.push_back(change);
            }
        }
        std::sort(changes.begin(),changes.end());
        const int compared=int(changes.size());
        const float medianChange=compared?changes[compared/2]:1000;
        const float p90Change=compared?changes[std::min(compared-1,int(std::ceil(compared*.90f))-1)]:1000;
        if(compared==0||medianChange>4.0f||p90Change>10.0f)stableProbes=0;
        else ++stableProbes;
        static float nextRainWaitReport=0;
        if(now>=nextRainWaitReport&&stableProbes<3) {
            nextRainWaitReport=now+2;
            std::cout<<"[RAIN WAIT] stable="<<stableProbes<<"/3 cells="<<compared
                     <<" medianChange="<<medianChange<<" p90Change="<<p90Change<<std::endl;
        }
        if(stableProbes<3) {grid.rainRate=0;raining=false;return;}
        // Three consistent probes after the hand-clear settling delay.
        std::cout<<"[RAIN] sand depth stable; starting the 30-second storm"<<std::endl;
        int footprintCells=0;
        for(size_t i=0;i<stormFootprint.size();++i) {
            if(stormFootprint[i] && grid.valid[i]) {
                ++footprintCells;
            }
        }
        stormPending=false;
        if(footprintCells==0) {
            stormEnd=now;raining=false;
            std::cout<<"[RAIN] no valid ground under storm footprint"<<std::endl;
            return;
        }
        // A conservative full-cloud ceiling prevents edge clipping from
        // concentrating the whole storm's budget into the remaining cells.
        const float cellArea=roi.width*roi.height/(grid.width*grid.height);
        const float extent=patchRadius*1.2f;
        stormBudget=std::max(float(footprintCells),std::ceil(4*extent*extent/cellArea))*16.0f;
        stormAdded=0;stormStart=now;stormEnd=now+stormDuration;
        capturing=false;visitor=false;cloud.start(now);nextCloudMesh=0;
        std::cout<<"[RAIN V7] terrain rescanned; "<<footprintCells
                 <<" source cells inside ONE cloud; no elevation filter; 30 seconds"<<std::endl;
    }
    const bool wasRaining=raining;
    raining=stormStart>-999 && now>=stormStart && now<stormEnd;
    if(!raining) {
        grid.rainRate=0;
        if(wasRaining) {
            // Finish this weather cycle immediately, without clearing pools.
            thunder.stop();holdAudioStarted=false;wavStartedAt=-1000;nextWavFlash=0;
            heldSince=-1;lastHandTime=-1000;handLatched=false;
            stormPending=false;stormStart=-1000;stormEnd=-1000;
            handClearSince=-1;stableProbes=0;
            std::fill(stormFootprint.begin(),stormFootprint.end(),0);
            makeVisitor(now);updateCloud(now);
            std::cout<<"[RAIN V9] rain ended; new small cloud elsewhere; pools continue draining"<<std::endl;
        }
        return;
    }
    const float elapsed=now-stormStart;
    const float ramp=ofClamp(elapsed/4.0f,0.0f,1.0f);
    // Reuse the moving footprint for both high and low ground.
    stormFootprint=rainPatch;
    // Every valid point inside the cloud rains equally, regardless of elevation.
    for(size_t i=0;i<rain.size();++i) {
        if(stormFootprint[i] && grid.valid[i]) {
            rain[i]=1;++activeSources;
        }
    }
    // Keep v6's approximately 16 mm / 30 s local intensity. A larger
    // cloud rains on more land; leaving the box never boosts the rate.
    const float peakRate=16.0f/std::max(1.0f,stormDuration-2.0f);
    grid.rainRate=peakRate*ramp*ramp*(3.0f-2.0f*ramp);
}

void RainRunoff::sampleTerrain(bool force)
{
    // LIVE_DRAINAGE_V3: refresh from the existing unfiltered CPU snapshot.
    // Filtered terrain can retain a smoothed-over wall after a narrow breach.
    const float terrainNow=ofGetElapsedTimef();
    uint64_t terrainSequence=0;
    if(projector->copyRainDepthSnapshot(raw,terrainSequence)&&terrainSequence!=rawSequence) {
        rawSequence=terrainSequence;rawFreshTime=terrainNow;
    }
    if(!raw.isAllocated()||terrainNow-rawFreshTime>.45f)return;
    if(terrainCandidate.size()!=grid.ground.size()) {
        terrainCandidate.assign(grid.ground.size(),0);
        terrainCandidateAt.assign(grid.ground.size(),-1);
    }
    int terrainChanges=0,loweredCells=0,raisedCells=0;
    // Preserve the last ground sample under a raised hand.
    for(int y=0;y<grid.height;++y) for(int x=0;x<grid.width;++x) {
        const int i=y*grid.width+x;
        if(grid.valid[i] && !force &&
           (hand[i] || interactiveWater.protectsTerrainCell(i,terrainNow) || (stormPending && stormFootprint[i]))) {
            terrainCandidateAt[i]=-1;continue;
        }
        const float kx=roi.x+(x+.5f)*roi.width/grid.width;
        const float ky=roi.y+(y+.5f)*roi.height/grid.height;
        const float sx=roi.width/grid.width*.27f,sy=roi.height/grid.height*.27f;
        const float dx[]={0,sx,-sx,0,0},dy[]={0,0,0,sy,-sy};
        float samples[5];int count=0;
        for(int q=0;q<5;++q) {
            int px=int(kx+dx[q]),py=int(ky+dy[q]);
            if(px<0||py<0||px>=int(raw.getWidth())||py>=int(raw.getHeight()))continue;
            const float depth=raw[py*raw.getWidth()+px];
            if(depth<450||depth>4500)continue;
            const float e=projector->rainElevationFromDepth(px,py,depth);
            if(std::isfinite(e)&&e>-700&&e<MagicSandTriggers::values().handFloor)samples[count++]=e;
        }
        if(count>=3) {
            for(int a=1;a<count;++a) {
                const float value=samples[a];int b=a;
                while(b>0 && samples[b-1]>value) {samples[b]=samples[b-1];--b;}
                samples[b]=value;
            }
            // A robust low sample captures narrow channels without using
            // a single noisy pixel as a false drainage outlet.
            float next=samples[1];
            if(grid.valid[i]&&!force) {
                if(std::fabs(next-grid.ground[i])<1.5f){terrainCandidateAt[i]=-1;continue;}
                if(terrainCandidateAt[i]<0||std::fabs(next-terrainCandidate[i])>4) {
                    terrainCandidate[i]=next;terrainCandidateAt[i]=terrainNow;continue;
                }
                terrainCandidate[i]=.5f*(terrainCandidate[i]+next);
                if(terrainNow-terrainCandidateAt[i]<.24f)continue;
                next=terrainCandidate[i];
            }
            if(grid.valid[i]) {
                if(next<grid.ground[i]-1.5f)++loweredCells;
                if(next>grid.ground[i]+1.5f)++raisedCells;
            }
            if(grid.valid[i]&&!force)grid.terrainChanged(i,grid.ground[i],next);
            ++terrainChanges;grid.ground[i]=next;grid.valid[i]=1;terrainCandidateAt[i]=-1;
        } else terrainCandidateAt[i]=-1;
    }
    if(terrainChanges>0&&grid.total()>.01f&&terrainNow>=nextTerrainReport) {
        nextTerrainReport=terrainNow+1;
        std::cout<<"[RAIN TERRAIN] updated="<<terrainChanges<<" lowered="<<loweredCells
                 <<" raised="<<raisedCells<<" water="<<grid.total()<<std::endl;
    }
    for(int y=0;y<=grid.height;++y) for(int x=0;x<=grid.width;++x) {
        const float kx=roi.x+x*roi.width/grid.width;
        const float ky=roi.y+y*roi.height/grid.height;
        const ofVec2f p=projector->kinectCoordToProjCoord(kx,ky);
        if(std::isfinite(p.x)&&std::isfinite(p.y)) corners[y*(grid.width+1)+x]=p;
    }
    // Cloud geometry uses these cached corners on its own 5 Hz schedule.
}

float RainRunoff::cornerWater(int x,int y) const
{
    float sum=0;int count=0;
    for(int yy=y-1;yy<=y;++yy) for(int xx=x-1;xx<=x;++xx) {
        if(xx<0||yy<0||xx>=grid.width||yy>=grid.height) continue;
        const int i=yy*grid.width+xx;
        if(grid.valid[i]) {sum+=grid.water[i];++count;}
    }
    return count ? sum/count : 0;
}

void RainRunoff::buildMeshes(float)
{
    // BLUE_RAIN_POOLS_V27: visible film, with a second depth-blended pool layer.
    waterMesh.clear();poolMesh.clear();
    waterMesh.setMode(OF_PRIMITIVE_TRIANGLES);poolMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    if(surfaceLook.corners.size()!=corners.size()||surfaceLook.poolCorners.size()!=corners.size())return;
    auto vertex=[&](ofMesh& mesh,int x,int y,bool pooled){
        const int i=y*(grid.width+1)+x;const auto& p=corners[i];
        const auto& c=pooled?surfaceLook.poolCorners[i]:surfaceLook.corners[i];
        mesh.addVertex(ofVec3f(p.x,p.y,0));mesh.addColor(ofFloatColor(c.r,c.g,c.b,c.a));
        if(pooled)mesh.addTexCoord(ofVec2f(float(x),float(y)));
    };
    auto tile=[&](ofMesh& mesh,int x,int y,bool pooled){
        vertex(mesh,x,y,pooled);vertex(mesh,x+1,y,pooled);vertex(mesh,x,y+1,pooled);
        vertex(mesh,x+1,y,pooled);vertex(mesh,x+1,y+1,pooled);vertex(mesh,x,y+1,pooled);
    };
    for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x){
        const int i=y*grid.width+x;if(!grid.valid[i])continue;
        const int c=y*(grid.width+1)+x;
        auto opacity=[&](const std::vector<RainSurfaceLook::Color>& colors){return
            std::max(std::max(colors[c].a,colors[c+1].a),std::max(colors[c+grid.width+1].a,colors[c+grid.width+2].a));};
        if(opacity(surfaceLook.corners)>.002f)tile(waterMesh,x,y,false);
        if(opacity(surfaceLook.poolCorners)>.002f)tile(poolMesh,x,y,true);
    }
    static float reportAt=-1000;
    const float now=ofGetElapsedTimef();
    if(now>=reportAt){reportAt=now+4;int pools=0,films=0;float maxDepth=0;
        for(size_t i=0;i<grid.water.size();++i)if(grid.valid[i]){
            float d=grid.water[i];maxDepth=std::max(maxDepth,d);if(d>=1)++pools;else if(d>.02f)++films;
        }
        std::cout<<"[RAIN SURFACE V27] poolCells="<<pools<<" filmCells="<<films<<" maxDepthMm="<<maxDepth<<std::endl;
    }
}


void RainRunoff::draw()
{
    if(!enabled) return;
    ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);
    waterMesh.draw();
    // WAVE_RENDER_V71: one consistent base tint on either side of the toggle.
    // Regular sea keeps its mapped terrain colors; pooled rain keeps its mesh
    // tint. No separate caustic artwork or hardcoded sea-blue replacement.
    poolMesh.draw();
    interactiveWater.draw(ofGetElapsedTimef());
    ofPopStyle();
}


void RainRunoff::drawFlash()
{
    // Final compositor pass: overhead cloud stays above trees and animals.
    if(enabled) {
        const GLboolean cloudDepth=glIsEnabled(GL_DEPTH_TEST),cloudCull=glIsEnabled(GL_CULL_FACE),cloudStencil=glIsEnabled(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
        ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);
    if(rainArmed && cloud.active) {
        if(cloudTextureLoaded)cloudSmoke.getTexture().bind();
        cloudMesh.draw();
        if(cloudTextureLoaded)cloudSmoke.getTexture().unbind();
    }
        ofPopStyle();
        if(cloudDepth)glEnable(GL_DEPTH_TEST);
        if(cloudCull)glEnable(GL_CULL_FACE);
        if(cloudStencil)glEnable(GL_STENCIL_TEST);
    }
    const float remaining=flashUntil-ofGetElapsedTimef();
    if(!enabled || remaining<=0 || corners.empty()) return;
    const float alpha=ofClamp(remaining/.30f,0.0f,1.0f)*245.0f;
    const ofVec2f a=corners[0];
    const ofVec2f b=corners[grid.width];
    const ofVec2f c=corners.back();
    const ofVec2f d=corners[grid.height*(grid.width+1)];
    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST);
    const GLboolean stencil=glIsEnabled(GL_STENCIL_TEST);
    const GLboolean cull=glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);
    ofPushStyle();ofEnableAlphaBlending();ofFill();
    ofSetColor(255,255,255,static_cast<int>(alpha));
    ofDrawTriangle(a.x,a.y,b.x,b.y,c.x,c.y);
    ofDrawTriangle(a.x,a.y,c.x,c.y,d.x,d.y);
    ofPopStyle();
    if(depth) glEnable(GL_DEPTH_TEST);
    if(stencil) glEnable(GL_STENCIL_TEST);
    if(cull) glEnable(GL_CULL_FACE);
}


float RainRunoff::cornerActivity(int x,int y) const
{
    float rate=0;int count=0;
    for(int yy=y-1;yy<=y;++yy) for(int xx=x-1;xx<=x;++xx) {
        if(xx<0||yy<0||xx>=grid.width||yy>=grid.height) continue;
        const int i=yy*grid.width+xx;
        if(grid.valid[i] && grid.water[i]>.0001f) {
            rate+=std::sqrt(grid.flowX[i]*grid.flowX[i]+grid.flowY[i]*grid.flowY[i])/.025f;
            ++count;
        }
    }
    return count ? ofClamp(rate/count*2.0f,0.0f,1.0f) : 0;
}


void RainRunoff::updateWavFlashes(float now)
{
    if(!holdAudioStarted || wavStartedAt<0) return;
    const float times[]={9.0f,21.0f,26.0f};
    const float elapsed=now-wavStartedAt;
    while(nextWavFlash<3 && elapsed>=times[nextWavFlash]) {
        // Timer is independent of rain start or hand-removal delays.
        flashUntil=now+.40f;
        std::cout<<"[RAIN] WAV +"<<times[nextWavFlash]<<" s: lightning"<<std::endl;
        ++nextWavFlash;
    }
}

void RainRunoff::makeVisitor(float now)
{
    const float sensing=ofClamp(std::min(roi.width,roi.height)*.065f,20.0f,32.0f);
    visitorRadius=sensing*.17f*std::sqrt(6.0f);
    patchRadius=3*visitorRadius;
    const bool relocating=cloud.active;
    const float oldX=cloud.x,oldY=cloud.y,oldOriginX=cloud.originX,oldOriginY=cloud.originY;
    float x=roi.x+roi.width*ofRandom(.20f,.80f);
    float y=roi.y+roi.height*ofRandom(.40f,.85f);
    if(relocating) {
        const float separation=std::max(visitorRadius*3.0f,std::min(roi.width,roi.height)*.24f);
        float bestScore=-1;
        // Prefer a random fresh location; fixed corner candidates guarantee
        // a useful fallback even if repeated random choices land nearby.
        for(int attempt=0;attempt<20;++attempt) {
            float px=attempt<16?roi.x+roi.width*ofRandom(.20f,.80f):roi.x+roi.width*((attempt%2)? .85f:.15f);
            float py=attempt<16?roi.y+roi.height*ofRandom(.40f,.85f):roi.y+roi.height*((attempt<18)? .40f:.85f);
            float d1=(px-oldX)*(px-oldX)+(py-oldY)*(py-oldY);
            float d2=(px-oldOriginX)*(px-oldOriginX)+(py-oldOriginY)*(py-oldOriginY);
            float score=std::min(d1,d2);
            if(score>bestScore){bestScore=score;x=px;y=py;}
            if(score>=separation*separation)break;
        }
    }
    float wind=ofRandom(6.283185307f),speed=std::min(roi.width,roi.height)/180;
    cloud.form(x,y,visitorRadius,ofRandom(6.283185307f),std::cos(wind)*speed,std::sin(wind)*speed);
    cloud.start(now);visitor=true;capturing=false;cloudCommitted=false;
    patchChosen=false;patchCenters.clear();heldSince=-1;nextCloudMesh=0;
    std::fill(rainPatch.begin(),rainPatch.end(),0);
    std::cout<<"[RAIN V8] small cloud ready: hold a raised hand at its location for five seconds"<<std::endl;
}

void RainRunoff::releaseVisitor(float now)
{
    cloud.radius=visitorRadius;cloud.originX=cloud.x;cloud.originY=cloud.y;
    cloud.driftAt=now;cloud.wanderAt=-1;visitor=true;capturing=false;cloudCommitted=false;
    patchChosen=false;nextCloudMesh=0;
    thunder.stop();holdAudioStarted=false;wavStartedAt=-1000;nextWavFlash=0;
    std::fill(rainPatch.begin(),rainPatch.end(),0);
}

void RainRunoff::makeRainPatch()
{
    if(patchChosen||!cloud.active)return;
    // Grow exactly at the captured visitor, without selecting a random hand cell.
    patchCenters.assign(1,ofVec2f(cloud.x,cloud.y));
    patchChosen=true;nextCloudMesh=0;updateCloud(ofGetElapsedTimef());
}


void RainRunoff::updateCloud(float now)
{
    if(now<nextCloudMesh)return;
    nextCloudMesh=now+.20f;
    if(!cloud.active) {
        if(cloudMesh.getNumVertices()>0)cloudMesh.clear();
        return;
    }
    if(visitor)cloud.wander(now,roi.x,roi.y,roi.width,roi.height);
    else if(capturing) {
        float u=heldSince<0?0:ofClamp((now-heldSince)/5.0f,0.0f,1.0f);
        float growth=u*u*(3-2*u);
        cloud.radius=cloudCommitted?patchRadius:visitorRadius+(patchRadius-visitorRadius)*growth;
    } else cloud.advance(now,roi.x,roi.y,roi.width,roi.height);
    std::fill(rainPatch.begin(),rainPatch.end(),0);
    if(cloud.active&&!visitor)for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x) {
        float kx=roi.x+(x+.5f)*roi.width/grid.width;
        float ky=roi.y+(y+.5f)*roi.height/grid.height;
        if(cloud.covers(kx,ky))rainPatch[y*grid.width+x]=1;
    }
    rebuildPatchGuide();
}

ofVec2f RainRunoff::projectCloudPoint(float kx,float ky) const
{
    float gx=ofClamp((kx-roi.x)*grid.width/roi.width,0.0f,float(grid.width));
    float gy=ofClamp((ky-roi.y)*grid.height/roi.height,0.0f,float(grid.height));
    int x=std::min(int(gx),grid.width-1),y=std::min(int(gy),grid.height-1);
    float u=gx-x,v=gy-y;
    return corners[y*(grid.width+1)+x]*((1-u)*(1-v))+
        corners[y*(grid.width+1)+x+1]*(u*(1-v))+
        corners[(y+1)*(grid.width+1)+x]*((1-u)*v)+
        corners[(y+1)*(grid.width+1)+x+1]*(u*v);
}

void RainRunoff::rebuildPatchGuide()
{
    // The old circle guide is now one cached, smoke-textured triangle batch.
    cloudMesh.clear();cloudMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    if(!cloud.active||corners.size()!=size_t((grid.width+1)*(grid.height+1)))return;
    struct Point {float x,y,u,v;};
    auto clipped=[&](std::vector<Point> poly) {
        // Clip in Kinect space, interpolating UVs, instead of clamping a
        // whole off-box smoke quad into a bright/squashed strip at the edge.
        for(int edge=0;edge<4;++edge) {
            std::vector<Point> out;
            auto distance=[&](const Point& p) {
                return edge==0?p.x-roi.x:edge==1?roi.x+roi.width-p.x:
                       edge==2?p.y-roi.y:roi.y+roi.height-p.y;
            };
            if(poly.empty())break;
            Point previous=poly.back();float dp=distance(previous);
            for(const auto& point:poly) {
                float d=distance(point);
                if((d>=0)!=(dp>=0)) {
                    float t=dp/(dp-d);
                    out.push_back({previous.x+(point.x-previous.x)*t,
                        previous.y+(point.y-previous.y)*t,
                        previous.u+(point.u-previous.u)*t,previous.v+(point.v-previous.v)*t});
                }
                if(d>=0)out.push_back(point);
                previous=point;dp=d;
            }
            poly.swap(out);
        }
        return poly;
    };
    const float now=ofGetElapsedTimef();
    const float breathe=1+.025f*std::sin(now*.7f);
    for(size_t n=0;n<cloud.lobes.size();++n) {
        const auto& lobe=cloud.lobes[n];float cx,cy;cloud.lobeCenter(lobe,cx,cy);
        float angle=cloud.angle+lobe.angle;
        float ca=std::cos(angle),sa=std::sin(angle);
        float rx=cloud.radius*lobe.rx*breathe,ry=cloud.radius*lobe.ry*breathe;
        float growth=ofClamp((cloud.radius-visitorRadius)/std::max(1.0f,patchRadius-visitorRadius),0.0f,1.0f);
        // SMOKY_CLOUDS_V2: white visitor; neutral-gray storm. Retain
        // the smoke PNG alpha instead of drawing solid painted puffs.
        float shade=1.0f-.50f*growth+.018f*growth*float(int(n%3)-1);
        float cloudAlpha=.65f-.10f*growth;
        auto emit=[&](const Point& p,float alpha) {
            auto position=projectCloudPoint(p.x,p.y);
            cloudMesh.addVertex(ofVec3f(position.x,position.y,0));
            cloudMesh.addColor(ofFloatColor(shade,shade,shade,alpha));
            if(cloudTextureLoaded)cloudMesh.addTexCoord(cloudSmoke.getTexture().getCoordFromPercent(p.u,p.v));
        };
        if(cloudTextureLoaded) {
            std::vector<Point> quad;
            const float xs[]={-1,1,1,-1},ys[]={-1,-1,1,1};
            for(int j=0;j<4;++j)quad.push_back({cx+xs[j]*rx*ca-ys[j]*ry*sa,
                cy+xs[j]*rx*sa+ys[j]*ry*ca,(xs[j]+1)*.5f,(ys[j]+1)*.5f});
            quad=clipped(quad);
            for(size_t j=1;j+1<quad.size();++j){emit(quad[0],cloudAlpha);emit(quad[j],cloudAlpha);emit(quad[j+1],cloudAlpha);}
        } else {
            // Missing asset fallback remains a soft irregular overhead cloud.
            for(int j=0;j<12;++j) {
                float a=j*6.283185307f/12,b=(j+1)*6.283185307f/12;
                std::vector<Point> tri{{cx,cy,1,0},
                    {cx+std::cos(a)*rx*ca-std::sin(a)*ry*sa,cy+std::cos(a)*rx*sa+std::sin(a)*ry*ca,0,0},
                    {cx+std::cos(b)*rx*ca-std::sin(b)*ry*sa,cy+std::cos(b)*rx*sa+std::sin(b)*ry*ca,0,0}};
                tri=clipped(tri);
                for(size_t k=1;k+1<tri.size();++k){emit(tri[0],tri[0].u*cloudAlpha);emit(tri[k],tri[k].u*cloudAlpha);emit(tri[k+1],tri[k+1].u*cloudAlpha);}
            }
        }
    }
}



void RainRunoff::setRainEnabled(bool allowed)
{
    if(rainArmed==allowed) return;
    rainArmed=allowed;
    // Disarming cancels only the storm, preserving existing water and flow.
    thunder.stop();heldSince=-1;lastHandTime=-1000;lastOcclusionTime=-1000;
    handLatched=false;stormPending=false;raining=false;stormStart=-1000;stormEnd=-1000;
    handClearSince=-1;stableProbes=0;holdAudioStarted=false;wavStartedAt=-1000;
    nextWavFlash=0;flashUntil=-1000;patchChosen=false;patchCenters.clear();cloudMesh.clear();cloud.reset();visitor=false;capturing=false;cloudCommitted=false;
    std::fill(hand.begin(),hand.end(),0);std::fill(rain.begin(),rain.end(),0);
    std::fill(rainPatch.begin(),rainPatch.end(),0);
    grid.rainRate=0;activeSources=0;nextScan=0;
    std::cout<<"[RAIN] feature "<<(allowed ? "ON: small visitor cloud ready" : "OFF: pools still drain")<<std::endl;
}

// IDLE_DEMO_V1: a separate RainRunoff instance owns every demo pool.
bool RainRunoff::beginDemo(const std::shared_ptr<KinectProjector>& k,float x,float y) {
    demoMode=false;setRainEnabled(true);update(k,true);
    if(!enabled)return false;
    float now=ofGetElapsedTimef();
    // Anchor the visitor at the current live cloud location, then keep it
    // still while the pictured hand demonstrates a five-second hold.
    cloud.x=cloud.originX=ofClamp(x,roi.x+visitorRadius*1.2f,roi.x+roi.width-visitorRadius*1.2f);
    cloud.y=cloud.originY=ofClamp(y,roi.y+visitorRadius*1.2f,roi.y+roi.height-visitorRadius*1.2f);
    cloud.driftAt=-1;cloud.wanderAt=-1;
    visitor=false;capturing=true;heldSince=now;demoBegan=now;
    demoMode=true;demoTriggered=false;nextCloudMesh=0;
    return true;
}
void RainRunoff::advanceDemo(float now) {
    uint64_t sequence=0;
    if(projector->copyRainDepthSnapshot(raw,sequence)&&sequence!=rawSequence){rawSequence=sequence;rawFreshTime=now;}
    std::fill(hand.begin(),hand.end(),0);
    if(demoTriggered)return;
    if(now-demoBegan>=4&&!holdAudioStarted) {
        patchChosen=false;makeRainPatch();holdAudioStarted=true;wavStartedAt=now;nextWavFlash=0;
        if(soundLoaded){thunder.stop();thunder.setPosition(0);thunder.play();}
    }
    if(now-demoBegan>=5) {
        handLatched=true;cloudCommitted=true;cloud.radius=patchRadius;nextCloudMesh=0;
        updateCloud(now);stormFootprint=rainPatch;stormPending=true;rescanAt=now+1.4f;
        handClearSince=-1;stableProbes=0;nextStableProbe=0;flashUntil=now+.4f;
        lastOcclusionTime=-1000;demoTriggered=true;
    }
}
void RainRunoff::endDemo() {
    thunder.stop();demoMode=false;demoTriggered=false;demoBegan=-1000;
    setRainEnabled(false);
    grid.clear();waterMesh.clear();cloudMesh.clear();cloud.reset();
    enabled=false;lastUpdate=-1;
}
