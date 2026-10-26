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
#pragma once
#include "LavaSurfaceMotion.h" // LAVA_DISTORTION_V30
#include "ofMain.h"
#include "../KinectProjector/KinectProjector.h"
#include "VolcanoGrid.h"
#include "../TriggerSettings.h"
#include <memory>
#include <vector>

class Volcano {
public:
    void setSoundEnabled(bool yes){audioEnabled=yes;warningSound.setVolume(yes?.8f:0);eruptionSound.setVolume(yes?.75f:0);}
    void setEnabled(bool allowed);
    void update(const std::shared_ptr<KinectProjector>& k,bool normalMode);
    void draw();
    bool isBusy() const { return trigger.warning||trigger.erupting; }
    bool beginDemo(const std::shared_ptr<KinectProjector>& k);
    void endDemo();
    ofVec2f demoScreenPosition() const {return trigger.vent>=0?project(cellCenter(trigger.vent).x,cellCenter(trigger.vent).y):ofVec2f();}
private:
    bool audioEnabled=true;
    bool demoMode=false; // IDLE_DEMO_V1: isolated effects, no live tree-fire mutation
    bool armed=false,active=false,assetsTried=false;
    bool lavaLoaded=false,smokeLoaded=false,soundLoaded=false;
    VolcanoGrid grid;
    VolcanoTrigger trigger;
    ofRectangle roi;
    std::shared_ptr<KinectProjector> projector;
    ofShortPixels raw;
    uint64_t sequence=0;
    float rawFreshAt=-1000,lastUpdate=-1,nextScan=0,nextMesh=0,accumulator=0;
    float windAngle=0,nextSmoke=0,peakHoldRelief=0;
    int raisedCells=0,moundBroad=0,moundLower=0,moundRim=0;
    float sandReference=0,handCeiling=280;
    float lastPeakTarget=0,lastHandFloor=280;
    float eruptionResetHeight=0,belowResetSince=-1;
    bool volcanoSleeping=false;
    float lastInteraction=-1;
    int resetMotionCells=0;
    std::vector<float> interactionHeights; // AUTO_VOLCANO_V1
    std::string moundReason="waiting";
    std::vector<unsigned char> blocked;
    std::vector<ofVec2f> corners;
    std::vector<int> sources;
    ofVboMesh hotMesh,crustMesh,smokeMesh;
    struct LavaRenderSample {ofVec2f position;ofFloatColor base,hot;float volume;};
    std::vector<LavaRenderSample> lavaRenderSamples;
    std::vector<ofVec2f> renderPositions,renderUV;
    bool renderMapDirty=true,lavaMeshDirty=true;
    float nextTerrainScan=0,nextSmokeMesh=0;
    ofImage lavaImage,smokeImage;
    LavaSurfaceMotion lavaSurfaceMotion; // LAVA_DISTORTION_V30
    ofSoundPlayer warningSound,eruptionSound;
    bool eruptionSoundLoaded=false,eruptionSoundActive=false;
    struct Puff {ofVec2f position;float born,size,rotation;};
    std::vector<Puff> smoke;
    void rebuild(const ofRectangle& r);
    void scan(float now);
    bool evaluateMoundAt(int peak,float& prominence);
    bool findMound(int& peak,float& prominence,bool& lowered);
    void makeSources();
    void settleColdLava();
    void buildMeshes();
    void buildSmokeMesh(float now);
    ofVec2f project(float x,float y) const;
    ofVec2f cellCenter(int i) const;
};
