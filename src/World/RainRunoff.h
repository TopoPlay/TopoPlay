/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#include "RainPoolShimmer.h" // BLUE_RAIN_POOLS_V27
#include "RainSurfaceLook.h" // RAIN_SATURATION_V26
#include "InteractiveWater.h" // WATER_RIPPLES_V60
#pragma once
#include "ofMain.h"
#include "../KinectProjector/KinectProjector.h"
#include "RainWaterGrid.h"
#include "StormCloud.h"
#include "../TriggerSettings.h"
#include <memory>
#include <vector>

class RainRunoff {
public:
    void setSoundEnabled(bool yes){audioEnabled=yes;thunder.setVolume(yes?.85f:0);}
    void update(const std::shared_ptr<KinectProjector>& k,bool active);
    void draw();
    void drawFlash();
    void setRainEnabled(bool allowed);
    bool isRainEnabled() const { return rainArmed; }
    bool isBusy() const { return raining||stormPending||capturing; }
    ofVec2f cloudKinectPosition() const { return ofVec2f(cloud.x,cloud.y); }
    ofVec2f cloudScreenPosition() const { return projectCloudPoint(cloud.x,cloud.y); }
    bool beginDemo(const std::shared_ptr<KinectProjector>& k,float x,float y);
    bool demoFinished() const { return demoTriggered&&!raining&&!stormPending&&ofGetElapsedTimef()>demoBegan+7; }
    void endDemo();
private:
    bool audioEnabled=true;
    // Raw-height band, measured ABOVE the calibrated waterline.
    float lastHandFloor=280.0f;
    bool demoMode=false,demoTriggered=false;float demoBegan=-1000;
    void advanceDemo(float now);
    RainWaterGrid grid;
    RainSurfaceLook surfaceLook; // RAIN_SATURATION_V26
    InteractiveWater interactiveWater; // WATER_RIPPLES_V60
    ofRectangle roi;
    std::shared_ptr<KinectProjector> projector;
    ofShortPixels raw;
    uint64_t rawSequence=0;
    float rawFreshTime=-1000;
    std::vector<unsigned char> hand,rain,visited;
    std::vector<float> terrainCandidate,terrainCandidateAt; // LIVE_DRAINAGE_V3
    float nextTerrainReport=0;
    std::vector<ofVec2f> corners;
    ofMesh waterMesh;
    ofMesh poolMesh;RainPoolShimmer poolShimmer; // BLUE_RAIN_POOLS_V27
    ofVboMesh cloudMesh;
    ofImage cloudSmoke;
    bool cloudTextureTried=false,cloudTextureLoaded=false;
    StormCloud cloud;
    float nextCloudMesh=0;
    bool visitor=false,capturing=false,cloudCommitted=false;
    float visitorRadius=0;
    void makeVisitor(float now);
    void releaseVisitor(float now);
    void updateCloud(float now);
    ofVec2f projectCloudPoint(float x,float y) const;
    bool enabled=false,raining=false,soundTried=false,soundLoaded=false;
    bool handLatched=false,stormPending=false;
    bool holdAudioStarted=false;
    bool rainArmed=false,patchChosen=false;
    float wavStartedAt=-1000;
    int nextWavFlash=0;
    std::vector<unsigned char> rainPatch;
    std::vector<ofVec2f> patchCenters;
    float patchRadius=0;
    void makeRainPatch();
    void rebuildPatchGuide();
    void updateWavFlashes(float now);
    std::vector<unsigned char> stormFootprint;
    float rescanAt=-1000,stormStart=-1000,stormEnd=-1000;
    float handClearSince=-1,lastOcclusionTime=-1000,nextStableProbe=0;
    float stormDuration=30.0f;
    int stableProbes=0;
    std::vector<float> previousGround;
    float stormBudget=0,stormAdded=0;
    int activeSources=0;
    void updateStorm(float now);
    float lastUpdate=-1,accumulator=0,nextScan=0,nextTerrain=0,nextMesh=0;
    float heldSince=-1,lastHandTime=-1000,lastStorm=-1000,flashUntil=-1000;
    ofSoundPlayer thunder;
    void rebuild(const ofRectangle& area);
    void detectHand(float now);
    void sampleTerrain(bool force=false);
    void buildMeshes(float now);
    float cornerWater(int x,int y) const;
    float cornerActivity(int x,int y) const;
};
