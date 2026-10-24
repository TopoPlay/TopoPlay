/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
/* PROJECT_ATTRIBUTION_V45
 * Magic-Sand: Thomas Wolf and Rasmus R. Paulsen; based in part on
 * Oliver Kreylos / UC Davis / KeckCAVES SARndbox and Gene Kogan's calibration toolkit.
 * Educational extensions developed with operator testing and ChatGPT/Codex.
 * Existing file-level copyrights/licenses remain below. See CREDITS.md and ASSETS.md
 * for dependencies, generated demonstration/volcano media and credited audio.
 */
/***********************************************************************
ofApp.cpp - main openframeworks app
Copyright (c) 2016-2017 Thomas Wolf and Rasmus R. Paulsen (people.compute.dtu.dk/rapa)

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "ofApp.h"
#include "OperatorPanelScroll.h" // EDITOR_SCROLL_V58
#include "BoneButton.h" // BONE_BUTTON_V8
#include "AutoBonePlacement.h" // AUTO_BONE_V22
#include "FlexibleRing.h" // FLEXIBLE_RING_V4
// SMALL_RING_BASELINE_V18: restored confirmed v5 sensing and original placement.
#include <GLFW/glfw3.h>
#include <cmath>

void ofApp::beginShutdown()
{
    if(shuttingDown)return;
    shuttingDown=true;
    ofSoundStopAll();
    if(kinectProjector)kinectProjector->requestShutdown();
}

bool ofApp::shutdownComplete() const
{
    return !kinectProjector || kinectProjector->shutdownComplete();
}

void ofApp::releaseShutdownGraphics()
{
    // Called in the operator update callback while its context still exists.
    projectorFrameSync.release();
}

void ofApp::setup() {

    // ========================================================
    // POWER PROFILE
    //
    // MagicSand runs cooler on this laptop in balanced mode.
    // Force balanced mode whenever the application starts.
    // ========================================================

    int powerResult =
        ::system("powerprofilesctl set balanced");

    if (powerResult == 0)
    {
        std::cout
            << "[POWER] balanced mode enabled"
            << std::endl;
    }
    else
    {
        std::cout
            << "[POWER] WARNING: could not enable balanced mode"
            << std::endl;
    }

	// OF basics
	ofSetFrameRate(30);
	ofBackground(0);
    // MINIMIZED_WINDOW_V48: OF updates and draws both windows serially.
    // A vsynced operator-window swap can stall the visible projector when
    // the desktop compositor throttles the minimized operator window.
    // Keep pacing in CpuLimiter's saved FPS cap instead of display swaps.
    ofSetVerticalSync(false);
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetLogLevel("ofThread", OF_LOG_WARNING);
	ofSetLogLevel("ofFbo", OF_LOG_ERROR);
	ofSetLogLevel("ofShader", OF_LOG_ERROR);
	ofSetLogLevel("ofxKinect", OF_LOG_WARNING);

	// Setup kinectProjector
	kinectProjector = std::make_shared<KinectProjector>(projWindow);
	kinectProjector->setup(true);
	
	// Setup sandSurfaceRenderer
	sandSurfaceRenderer = new SandSurfaceRenderer(kinectProjector, projWindow);
	sandSurfaceRenderer->setup(true);
	
	// Retrieve variables
	ofVec2f kinectRes = kinectProjector->getKinectRes();
	ofVec2f projRes = ofVec2f(projWindow->getWidth(), projWindow->getHeight());

	// Final projector image.  Terrain, map and animals are composited
	// into this ONCE, then this same texture is shown on both displays.
	projectorComposite.allocate(
		static_cast<int>(projRes.x),
		static_cast<int>(projRes.y),
		GL_RGBA
	);

	projectorComposite.begin();
	ofClear(0, 0, 0, 255);
	projectorComposite.end();
    setupProjectorColor();
	ofRectangle kinectROI = kinectProjector->getKinectROI();
	mainWindowROI = ofRectangle((ofGetWindowWidth()-kinectRes.x)/2, (ofGetWindowHeight()-kinectRes.y)/2, kinectRes.x, kinectRes.y);

	mapGameController.setup(kinectProjector);
	mapGameController.setProjectorRes(projRes);
	mapGameController.setKinectRes(kinectRes);
	mapGameController.setKinectROI(kinectROI);

	boidGameController.setup(kinectProjector);
	boidGameController.setProjectorRes(projRes);
	boidGameController.setKinectRes(kinectRes);
	boidGameController.setKinectROI(kinectROI);


    cpuLimiter.setup(); // MAGICSAND_CPU_GUI_V1

    triggerControls.setup(); // TRIGGER_CONTROLS_V1

    idleDemo.setup(); // IDLE_DEMO_V1

    loadBoxSettings(); // BOX_SETTINGS_V7
    autoRunStartup.begin(ofFile::doesFileExist("settings/calibration.xml")); // AUTO_RUN_V32
}


void ofApp::update() {
    if(shuttingDown)return;
    layoutOperatorGui(); // EDGE_GUI_V6
    // Poll the animal/mode panel even during Fossil or Dig mode, when normal
    // animal simulation is skipped. Polling only from the boid update leaves
    // these mode lockout controls unresponsive.
    boidGameController.updateOperatorGui(); // MODE_BUTTONS_V37
    triggerControls.update(); // TRIGGER_CONTROLS_V1

    cpuLimiter.update(); // MAGICSAND_CPU_GUI_V1

    if(projectorColorGui && projectorColorVisible) projectorColorGui->update();
    if(projectorColorDirty && ofGetElapsedTimef()-projectorColorChangedAt>.5f)
        saveProjectorColor();
    static uint64_t profFrames = 0;
    static uint64_t profKinect = 0;
    static uint64_t profSand = 0;
    static uint64_t profMap = 0;
    static uint64_t profBoid = 0;
    static uint64_t profComposite = 0;
    static uint64_t profLastPrint = ofGetElapsedTimeMicros();

    uint64_t profA, profB;
    diagUpdateCount++;

    float diagNow = ofGetElapsedTimef();
    if (diagLastReport == 0.0f)
        diagLastReport = diagNow;

    if (diagNow - diagLastReport >= 2.0f)
    {
        float dt = diagNow - diagLastReport;

        std::cout
            << "[WINDOW-RATE] UPDATE="
            << (diagUpdateCount / dt)
            << " Hz  MAIN-DRAW="
            << (diagMainDrawCount / dt)
            << " Hz  PROJECTOR-DRAW="
            << (diagProjDrawCount / dt)
            << " Hz"
            << std::endl;

        auto glfwWindow =
            dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());

        if (glfwWindow)
        {
            GLFWwindow* nativeWindow = glfwWindow->getGLFWWindow();

            if (nativeWindow)
            {
                int visible =
                    glfwGetWindowAttrib(nativeWindow, GLFW_VISIBLE);

                int iconified =
                    glfwGetWindowAttrib(nativeWindow, GLFW_ICONIFIED);

                int focused =
                    glfwGetWindowAttrib(nativeWindow, GLFW_FOCUSED);

                std::cout
                    << "[WINDOW-STATE] visible="
                    << visible
                    << " iconified="
                    << iconified
                    << " focused="
                    << focused
                    << std::endl;
            }
        }

        diagUpdateCount = 0;
        diagMainDrawCount = 0;
        diagProjDrawCount = 0;
        diagLastReport = diagNow;
    }

    // Call kinectProjector->update() first during the update function()
    profA = ofGetElapsedTimeMicros();
	kinectProjector->update();
    // Native Kinect update must run first so startup sees the latest readiness.
    // Native RUN loads calibration and can reset the ROI/filter buffers again;
    // button placement below separately waits for the resulting stable depth.
    autoRunStartup.update(kinectProjector); // AUTO_RUN_V32
    updateBoxSettings(); // BOX_SETTINGS_V7: restore after RUN resets
    kinectProjector->traceCalibrationState("after native update/startup/profile/fit",true);
    ceilingMask.update(kinectProjector); // CONTROLS_V3

	// Very cheap projected ring-button experiment.
	updateRingButtonTest();

	// Full-ROI fossil mode.  This is separate from DIG mode and does
	// nothing unless the FOSSIL control is active.
	updateFossilFinder();

	// Excavation tracking is extremely light and is internally
	// throttled. It does nothing unless DIG mode is active.
	updateDigSites();
    idleDemo.update(kinectProjector,
        !fossilFinder.enabled&&!digSiteModeEnabled&&boidGameController.isIdle()&&mapGameController.isIdle()&&
        kinectProjector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING&&(idleDemo.running()||kinectProjector->isImageStabilized()), // DEMO_LOOP_V2
        rainRunoff.isBusy()||volcano.isBusy(),rainRunoff.cloudKinectPosition()); // IDLE_DEMO_V1


    // REGULAR_ANIMAL_DEFAULTS_V1: apply on entry, not every frame, so the
    // operator can still adjust the sliders during the current session.
    static bool regularAnimalDefaultsApplied=false;
    const bool regularAnimalsReady=
        !fossilFinder.enabled && !digSiteModeEnabled &&
        kinectProjector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING &&
        boidGameController.isIdle() && mapGameController.isIdle();
    if(!regularAnimalsReady)regularAnimalDefaultsApplied=false;
    else if(!regularAnimalDefaultsApplied) {
        if(!applyBoxAnimalDefaults()) { // BOX_SETTINGS_V7: saved requests take precedence
            boidGameController.setRequestedFishCount(200);
            boidGameController.setRequestedSharkCount(10);
        }
        regularAnimalDefaultsApplied=true;
        std::cout << (boxSettingsAvailable?"[REGULAR DEFAULTS] restored saved animal settings":"[REGULAR DEFAULTS] fish=200 sharks=10; stingrays follow sharks; habitat limits apply") << std::endl;
    }


    profB = ofGetElapsedTimeMicros();
    profKinect += profB - profA;

    // ========================================================
    // FOSSIL LIGHTWEIGHT MODE
    //
    // Fossil Finder only needs:
    //   Kinect
    //   excavation detection
    //   fossil image
    //   control buttons
    //
    // Do NOT spend CPU updating the normal terrain renderer,
    // map game, or animals while Fossil Mode is active.
    // ========================================================

    if (!fossilFinder.enabled)
    {
        profA = ofGetElapsedTimeMicros();
        sandSurfaceRenderer->update();
        profB = ofGetElapsedTimeMicros();
        profSand += profB - profA;
    }

    // Keep ROI information synchronized.
	if (kinectProjector->getKinectROI() != mapGameController.getKinectROI())
	{
		ofRectangle kinectROI = kinectProjector->getKinectROI();
		mapGameController.setKinectROI(kinectROI);
		boidGameController.setKinectROI(kinectROI);
	}

    if (!fossilFinder.enabled)
    {
        profA = ofGetElapsedTimeMicros();
        mapGameController.update();
        profB = ofGetElapsedTimeMicros();
        profMap += profB - profA;

        profA = ofGetElapsedTimeMicros();
        boidGameController.update();
        boidGameController.retryBoxMotherDefaults(); // BOX_SETTINGS_V7
        profB = ofGetElapsedTimeMicros();
        profBoid += profB - profA;
    }

    profA = ofGetElapsedTimeMicros();
    if(!idleDemo.running()) { // IDLE_DEMO_V1
    rainRunoff.setRainEnabled(!fossilFinder.enabled && !digSiteModeEnabled); // AUTO_VOLCANO_V1
    rainRunoff.update(kinectProjector,
        !fossilFinder.enabled && !digSiteModeEnabled &&
        kinectProjector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING);
    volcanoFeatureEnabled=!fossilFinder.enabled&&!digSiteModeEnabled; // AUTO_VOLCANO_V1
    volcano.setEnabled(volcanoFeatureEnabled);
    volcano.update(kinectProjector,
        !fossilFinder.enabled && !digSiteModeEnabled &&
        kinectProjector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING);
    }
	updateProjectorComposite();
    profB = ofGetElapsedTimeMicros();
    profComposite += profB - profA;

    profFrames++;

    uint64_t profNow = ofGetElapsedTimeMicros();

    if (profNow - profLastPrint >= 2000000 && profFrames > 0)
    {
        double n = static_cast<double>(profFrames);

        std::cout
            << "[UPDATE-PROFILE] "
            << "Kinect=" << (profKinect / n) / 1000.0 << "ms "
            << "Sand=" << (profSand / n) / 1000.0 << "ms "
            << "Map=" << (profMap / n) / 1000.0 << "ms "
            << "Boids=" << (profBoid / n) / 1000.0 << "ms "
            << "Composite=" << (profComposite / n) / 1000.0 << "ms "
            << std::endl;

        profFrames = 0;
        profKinect = 0;
        profSand = 0;
        profMap = 0;
        profBoid = 0;
        profComposite = 0;
        profLastPrint = profNow;
    }
}



// ============================================================
// Experimental projected ring button
//
// We already know where the projected button is, so there is
// no reason to search the entire Kinect image.
//
// At each angle we compare:
//
//          OUTSIDE
//             *
//             |
//        RING *       <- physical ring should be higher
//             |
//        INNER *
//
// A hand/arm usually raises all three samples together.
// A narrow physical ring should make the middle sample higher
// than both its inner and outer neighbours.
// ============================================================

void ofApp::updateRingButtonTest()
{
    // MODE_BUTTONS_V37: hiding a control also locks out its active mode.
    if(!MagicSandModeButtons::shown(0)&&fossilFinder.enabled)stopFossilFinder();
    if(!MagicSandModeButtons::shown(1)&&digSiteModeEnabled){digSiteModeEnabled=false;digSites.clear();digReferenceFrozen=false;}
    for(int slot=0;slot<2;++slot)if(!MagicSandModeButtons::shown(slot))ringButtons[slot]=RingButtonState{};
    if(!MagicSandModeButtons::shown(0)&&!MagicSandModeButtons::shown(1)){
        MagicSandAutoBone::current().ready=false;digButtonLayout.ready=false;return;
    }


    // TRIGGER_CONTROLS_V1: same raw waterline heights as rain; one CPU
    // snapshot copy shared by all physical buttons in this update.
    static ofShortPixels triggerRaw;
    static uint64_t triggerRawSequence=0;
    static float triggerRawAt=-1000;
    uint64_t triggerSequence=0;
    const float triggerNow=ofGetElapsedTimef();
    if(kinectProjector&&kinectProjector->copyRainDepthSnapshot(triggerRaw,triggerSequence)&&triggerSequence!=triggerRawSequence) {
        triggerRawSequence=triggerSequence;triggerRawAt=triggerNow;
    }
    auto triggerElevation=[&](float x,float y)->float {
        if(!kinectProjector||!triggerRaw.isAllocated()||triggerNow-triggerRawAt>.35f)
            return std::numeric_limits<float>::quiet_NaN();
        int ix=int(x),iy=int(y);
        if(ix<0||iy<0||ix>=int(triggerRaw.getWidth())||iy>=int(triggerRaw.getHeight()))
            return std::numeric_limits<float>::quiet_NaN();
        float depth=triggerRaw[iy*triggerRaw.getWidth()+ix];
        if(depth<450||depth>4500)return std::numeric_limits<float>::quiet_NaN();
        return kinectProjector->rainElevationFromDepth(x,y,depth);
    };
    static float triggerButtonSince[4]={-1,-1,-1,-1};
    static unsigned triggerRevision=0;
    if(triggerRevision!=MagicSandTriggers::revision()) {
        triggerRevision=MagicSandTriggers::revision();
        for(int i=0;i<4;++i){triggerButtonSince[i]=-1;ringButtons[i].consecutiveFrames=0;}
    }
 // TRIGGER_CONTROLS_V1

    if (!kinectProjector)
        return;

    if (kinectProjector->GetApplicationState() !=
        KinectProjector::APPLICATION_STATE_RUNNING)
    {
        for (auto& button : ringButtons)
        {
            button.consecutiveFrames = 0;
            button.releaseFrames = 0;
            button.match = 0.0f;
            button.matchedSections = 0;
        }

        return;
    }


    const ofRectangle roi =
        kinectProjector->getKinectROI();


    if (roi.width < 100.0f ||
        roi.height < 100.0f)
    {
        return;
    }


    // BONE_CORNER_V36: do not latch a position from pre-ROI startup frames.
    if(!kinectProjector->isImageStabilized()){
        auto& pendingLayout=MagicSandAutoBone::current();
        pendingLayout.ready=false;pendingLayout.initialized=false;
        digButtonLayout.ready=false;digButtonLayout.initialized=false;
        return;
    }

    // AUTO_BONE_V22: one live layout selects a visible location for both paths.
    ringButtonRadius=ofClamp(std::min(roi.width,roi.height)*.065f,20.f,32.f);
    const float screenW=projWindow?float(projWindow->getWidth()):float(projectorComposite.getWidth());
    const float screenH=projWindow?float(projWindow->getHeight()):float(projectorComposite.getHeight());
    const auto sensor=kinectProjector->getKinectRes();
    std::vector<float> mapping;
    for(const auto& ray:std::vector<ofVec3f>{{sensor.x*.25f,sensor.y*.25f,1000},
        {sensor.x*.75f,sensor.y*.25f,1000},{sensor.x*.5f,sensor.y*.75f,1000},{sensor.x*.5f,sensor.y*.5f,1400}}){
        const auto p=kinectProjector->kinectCoordToProjCoord(ray.x,ray.y,ray.z);
        mapping.push_back(p.x);mapping.push_back(p.y);
    }
    auto probe=[&](float x,float y){
        MagicSandAutoBone::Candidate c;c.x=x;c.y=y;
        auto valid=[](const ofVec3f& w){return std::isfinite(w.x)&&std::isfinite(w.y)&&std::isfinite(w.z)&&w.z>=450&&w.z<=4500;};
        const auto ground=kinectProjector->kinectCoordToWorldCoord(x,y);
        if(!valid(ground))return c;
        const auto center=kinectProjector->worldCoordToProjCoord(ground);
        const auto rw=kinectProjector->kinectCoordToWorldCoord(x+ringButtonRadius,y);
        const auto dw=kinectProjector->kinectCoordToWorldCoord(x,y+ringButtonRadius);
        const auto right=valid(rw)?kinectProjector->worldCoordToProjCoord(rw):kinectProjector->kinectCoordToProjCoord(x+ringButtonRadius,y,ground.z);
        const auto down=valid(dw)?kinectProjector->worldCoordToProjCoord(dw):kinectProjector->kinectCoordToProjCoord(x,y+ringButtonRadius,ground.z);
        c.px=center.x;c.py=center.y;c.depth=ground.z;
        c.rx=std::max(12.f,(right-center).length());c.ry=std::max(12.f,(down-center).length());
        c.valid=std::isfinite(c.px)&&std::isfinite(c.py)&&std::isfinite(c.rx)&&std::isfinite(c.ry);
        if(c.valid)c.valid=MagicSandBoneRoi::contains(*kinectProjector,roi,c); // BONE_CORNER_V36
        return c;
    };
    auto& layout=MagicSandAutoBone::update({roi.x,roi.y,roi.width,roi.height},screenW,screenH,
        sensor.x,sensor.y,ringButtonRadius,triggerNow,kinectProjector.get(),mapping,probe);
    static unsigned usedLayoutRevision=0;
    if(layout.revision!=usedLayoutRevision||!layout.ready){
        usedLayoutRevision=layout.revision;
        triggerButtonSince[0]=-1;ringButtons[0].consecutiveFrames=0;
        ringButtons[0].match=0;ringButtons[0].matchedSections=0;
        // Keep an existing press latched until the original release test clears it.
    }
    if(layout.ready)ringButtons[0].kinectCenter.set(layout.chosen.x,layout.chosen.y);
    if(MagicSandModeButtons::shown(1)){
        auto& digLayout=MagicSandAutoBone::update({roi.x,roi.y,roi.width,roi.height},screenW,screenH,
            sensor.x,sensor.y,ringButtonRadius,triggerNow,kinectProjector.get(),mapping,probe,&digButtonLayout,true);
        static unsigned digRevision=0;
        if(digLayout.revision!=digRevision||!digLayout.ready){
            digRevision=digLayout.revision;triggerButtonSince[1]=-1;ringButtons[1].consecutiveFrames=0;
            ringButtons[1].match=0;ringButtons[1].matchedSections=0;
        }
        if(digLayout.ready)ringButtons[1].kinectCenter.set(digLayout.chosen.x,digLayout.chosen.y);
    }else digButtonLayout.ready=false;

    static unsigned reportedRevision=0;static bool lastReady=false;static float lastPlacementReport=-1000;
    if(triggerNow-lastPlacementReport>=3||reportedRevision!=layout.revision||lastReady!=layout.ready){
        reportedRevision=layout.revision;lastReady=layout.ready;lastPlacementReport=triggerNow;
        std::cout<<"[BONE AUTO V22] ready="<<layout.ready<<" roi="<<roi.x<<","<<roi.y<<","<<roi.width<<","<<roi.height;
        if(layout.ready)std::cout<<" sensing="<<layout.chosen.x<<","<<layout.chosen.y
            <<" projected="<<layout.chosen.px<<","<<layout.chosen.py<<" depth="<<layout.chosen.depth;
        else std::cout<<" reason=no_valid_visible_location";
        std::cout<<std::endl;
    }
    if(!layout.ready&&!digButtonLayout.ready)return;

    const int sampleCount = 16;

    const float centerSampleRadius = ringButtonRadius * 0.16f;
    // RAISED_RING_V2: keep projected artwork unchanged; expand only sensing.
    const float ringSenseBase = ringButtonRadius * MagicSandTriggers::values().ringScale;

    // These are the values that worked in the successful
    // finger-circle test.
    // Finger-circle can hover above the sand.
    //
    // There is NO upper height limit. It simply needs to be
    // above the live sand reference measured at the center.
    

    // Keep only a very small local-shape check.
    //
    // The center-height test does most of the work rejecting
    // broad arms/hands, while this still slightly favors a
    // narrow ring-shaped object.
    


    for (int buttonIndex = 0;
         buttonIndex < 4;
         ++buttonIndex)
    {
        if(!MagicSandModeButtons::shown(buttonIndex)||(buttonIndex==0?!layout.ready:!digButtonLayout.ready))continue; // MODE_BUTTONS_V37

        RingButtonState& button =
            ringButtons[buttonIndex];


        // ====================================================
        // LIVE SAND REFERENCE
        //
        // Five samples near the center track the current sand
        // elevation even after kids add/remove sand.
        // ====================================================

        // FLEXIBLE_RING_V4: do not require a parallel or narrow ridge profile.
        const int matches=MagicSandRing::sections(triggerElevation,
            button.kinectCenter.x,button.kinectCenter.y,ringSenseBase,
            centerSampleRadius,MagicSandTriggers::values().handFloor);


        button.matchedSections =
            matches;

        button.match =
            static_cast<float>(matches) /
            static_cast<float>(sampleCount);


        // More forgiving finger-circle threshold:
        // 8 / 16 = 50 percent.
        //
        // We still require five consecutive detections before
        // a button actually activates.
        const bool ringFound =
            matches >= 8;


        if (ringFound)
        {
            button.releaseFrames = 0;


            if (!button.latched)
            {
                if(button.consecutiveFrames==0)triggerButtonSince[buttonIndex]=ofGetElapsedTimef();
                button.consecutiveFrames++;


                if (triggerButtonSince[buttonIndex]>=0 && ofGetElapsedTimef()-triggerButtonSince[buttonIndex]>=MagicSandTriggers::values().buttonHold)
                {
                    button.latched = true;


                    // ========================================
                    // BUTTON ACTIONS
                    //
                    // 0 = FOSSIL
                    // 1 = DIG
                    //
                    // These modes are mutually exclusive.
                    // ========================================

                    if (buttonIndex == 0)
                    {
                        if (fossilFinder.enabled)
                        {
                            stopFossilFinder();

                            std::cout
                                << "[RING CONTROL] FOSSIL OFF"
                                << std::endl;
                        }
                        else
                        {
                            // Shut off DIG first.
                            if (digSiteModeEnabled)
                            {
                                digSiteModeEnabled = false;
                                digSites.clear();
                                digReferenceFrozen = false;
                            }


                            // Remove animals from the sandbox.
                            boidGameController.
                                setRequestedFishCount(0);

                            boidGameController.
                                setRequestedRabbitCount(0);

                            boidGameController.
                                setRequestedSharkCount(0);


                            startFossilFinder();


                            std::cout
                                << "[RING CONTROL] FOSSIL ON"
                                << std::endl;
                        }
                    }
                    else if (buttonIndex == 1)
                    {
                        digSiteModeEnabled =
                            !digSiteModeEnabled;


                        if (digSiteModeEnabled)
                        {
                            // Fossil and DIG cannot run together.
                            if (fossilFinder.enabled)
                            {
                                stopFossilFinder();
                            }


                            // Remove animals from the sandbox.
                            boidGameController.
                                setRequestedFishCount(0);

                            boidGameController.
                                setRequestedRabbitCount(0);

                            boidGameController.
                                setRequestedSharkCount(0);


                            // Start normal DIG arming period.
                            digModeActivatedTime =
                                ofGetElapsedTimef();

                            digReferenceFrozen =
                                false;

                            createDigSites();
                        }
                        else
                        {
                            digSites.clear();

                            digReferenceFrozen =
                                false;
                        }


                        std::cout
                            << "[RING CONTROL] DIG MODE "
                            << (
                                digSiteModeEnabled
                                    ? "ON"
                                    : "OFF"
                               )
                            << std::endl;
                    }
                    else if(buttonIndex==3)
                    {
                        volcanoFeatureEnabled=!volcanoFeatureEnabled;
                        if(volcanoFeatureEnabled) {
                            rainFeatureEnabled=false;rainRunoff.setRainEnabled(false);
                            if(fossilFinder.enabled) stopFossilFinder();
                            digSiteModeEnabled=false;digSites.clear();digReferenceFrozen=false;
                        }
                        volcano.setEnabled(volcanoFeatureEnabled);
                        std::cout<<"[RING CONTROL] VOLCANO "
                                 <<(volcanoFeatureEnabled ? "ON" : "OFF")<<std::endl;
                    }

                }
            }
        }
        else
        {
            button.consecutiveFrames = 0;


            if (button.latched)
            {
                button.releaseFrames++;


                if (button.releaseFrames >= 10)
                {
                    button.latched = false;
                    button.releaseFrames = 0;
                }
            }
        }
    }

    if(fossilFinder.enabled || digSiteModeEnabled) {
        volcanoFeatureEnabled=false;volcano.setEnabled(false);
        rainFeatureEnabled=false;
        rainRunoff.setRainEnabled(false);
    }
}



void ofApp::drawRingButtonTest()
{
    if(!kinectProjector||kinectProjector->GetApplicationState()!=KinectProjector::APPLICATION_STATE_RUNNING)return;
    const auto roi=kinectProjector->getKinectROI();
    const float width=projWindow?float(projWindow->getWidth()):float(projectorComposite.getWidth());
    const float height=projWindow?float(projWindow->getHeight()):float(projectorComposite.getHeight());
    auto drawControl=[&](int slot,const MagicSandAutoBone::State& layout){
        if(!MagicSandModeButtons::shown(slot)||!layout.ready)return;
        if(!MagicSandAutoBone::same(layout.roi,{roi.x,roi.y,roi.width,roi.height})||width!=layout.width||height!=layout.height)return;
        if(!MagicSandAutoBone::fits(layout.chosen,width,height))return;
        const auto& c=layout.chosen;const bool pressed=ringButtons[slot].match>=.50f;
        if(slot==0)MagicSandBone::draw(ofVec2f(c.px,c.py),c.rx,c.ry,fossilFinder.enabled,pressed);
        else MagicSandProjectedButtons::drawDig(ofVec2f(c.px,c.py),c.rx,c.ry,digSiteModeEnabled,pressed);
    };
    drawControl(0,MagicSandAutoBone::current());drawControl(1,digButtonLayout);
}



// ============================================================
// DIG SITE SYSTEM
// ============================================================


// ------------------------------------------------------------
// Projected control buttons are a protected NO-DIG ZONE.
//
// This is deliberately larger than the finger-circle detector.
// It protects the controls plus a buffer around them.
// ------------------------------------------------------------

bool ofApp::isInsideButtonNoDigZone(
    const ofVec2f& position) const
{
    const float protectedRadius =
        ringButtonRadius * 2.5f;


    for (int i = 0; i < 4; ++i)
    {
        if(!MagicSandModeButtons::shown(i))continue; // MODE_BUTTONS_V37

        if (
            position.distance(
                ringButtons[i].kinectCenter
            ) < protectedRadius
           )
        {
            return true;
        }
    }


    return false;
}



// ------------------------------------------------------------
// Create excavation sites.
//
// Locations remain fixed until DIG mode is turned off.
// ------------------------------------------------------------

void ofApp::createDigSites()
{
    digSites.clear();

    if (!kinectProjector)
        return;


    const ofRectangle roi =
        kinectProjector->getKinectROI();


    if (roi.width < 100.0f ||
        roi.height < 100.0f)
    {
        return;
    }


    const float shortDimension =
        std::min(
            roi.width,
            roi.height
        );


    // Larger excavation area:
    //
    // radius   ~= 12% of short sandbox dimension
    // diameter ~= 24%
    //
    digSiteRadius =
        ofClamp(
            shortDimension * 0.12f,
            35.0f,
            65.0f
        );


    const int siteCount = 3;


    for (int i = 0;
         i < siteCount;
         ++i)
    {
        DigSiteState site;

        bool accepted = false;


        for (int attempt = 0;
             attempt < 250 && !accepted;
             ++attempt)
        {
            // Keep the entire excavation circle comfortably
            // inside the usable sandbox.
            site.position.set(
                ofRandom(
                    roi.x + digSiteRadius + 10.0f,
                    roi.x + roi.width -
                        digSiteRadius - 10.0f
                ),

                ofRandom(
                    roi.y + digSiteRadius + 10.0f,
                    roi.y + roi.height -
                        digSiteRadius - 10.0f
                )
            );


            accepted = true;


            // -----------------------------------------------
            // NO-DIG controls
            // -----------------------------------------------

            const float buttonProtection =
                digSiteRadius +
                ringButtonRadius * 2.5f;


            for (int b = 0; b < 4; ++b)
            {
        if(!MagicSandModeButtons::shown(b))continue; // MODE_BUTTONS_V37

                if (
                    site.position.distance(
                        ringButtons[b].kinectCenter
                    ) < buttonProtection
                   )
                {
                    accepted = false;
                    break;
                }
            }


            if (!accepted)
                continue;


            // -----------------------------------------------
            // LAND-ONLY DIG SITE TEST
            //
            // Water is below the terrain zero level.
            //
            // Require:
            //   1. center safely on land
            //   2. at least 80% of the excavation circle on land
            //
            // +3 mm gives us a small shoreline safety buffer.
            // This prevents an X from appearing in water or
            // sitting right on the water's edge.
            // -----------------------------------------------

            const float shorelineBuffer = 3.0f;

            const float centerElevation =
                kinectProjector->
                    elevationAtKinectCoord(
                        site.position.x,
                        site.position.y
                    );


            if (centerElevation <= shorelineBuffer)
            {
                accepted = false;
                continue;
            }


            int landSamples = 0;
            int totalSamples = 0;


            // Test center plus several rings around the
            // proposed excavation circle.
            const int angleSamples = 16;

            const float testRadii[3] =
            {
                digSiteRadius * 0.35f,
                digSiteRadius * 0.65f,
                digSiteRadius * 0.95f
            };


            // Center counts as one sample.
            totalSamples++;

            if (centerElevation > shorelineBuffer)
            {
                landSamples++;
            }


            for (float radius : testRadii)
            {
                for (int a = 0;
                     a < angleSamples;
                     ++a)
                {
                    const float angle =
                        TWO_PI *
                        static_cast<float>(a) /
                        static_cast<float>(angleSamples);


                    const ofVec2f testPosition(
                        site.position.x +
                            cos(angle) * radius,

                        site.position.y +
                            sin(angle) * radius
                    );


                    // If any test point somehow falls outside
                    // the ROI, treat it as unusable.
                    if (!roi.inside(testPosition))
                    {
                        totalSamples++;
                        continue;
                    }


                    const float elevation =
                        kinectProjector->
                            elevationAtKinectCoord(
                                testPosition.x,
                                testPosition.y
                            );


                    totalSamples++;


                    if (elevation >
                        shorelineBuffer)
                    {
                        landSamples++;
                    }
                }
            }


            const float landFraction =
                totalSamples > 0
                    ? static_cast<float>(
                          landSamples
                      ) /
                      static_cast<float>(
                          totalSamples
                      )
                    : 0.0f;


            if (landFraction < 0.80f)
            {
                accepted = false;
                continue;
            }


            // -----------------------------------------------
            // Keep excavation sites from overlapping.
            // -----------------------------------------------

            for (const auto& existing :
                 digSites)
            {
                if (
                    site.position.distance(
                        existing.position
                    ) <
                    digSiteRadius * 1.75f
                   )
                {
                    accepted = false;
                    break;
                }
            }
        }


        if (!accepted)
            continue;


        // ====================================================
        // Build a fixed sampling grid inside the excavation
        // circle.
        //
        // 11 x 11 bounding grid; only samples inside the circle
        // are kept.
        // ====================================================

        // 41 x 41 excavation sampling grid.
        //
        // Keep CPU cost controlled; visual smoothing is handled
        // by the alpha-mask rendering rather than by thousands
        // of additional depth samples.
        const int gridRadius = 20;

        const float step =
            digSiteRadius /
            static_cast<float>(gridRadius);


        for (int gy = -gridRadius;
             gy <= gridRadius;
             ++gy)
        {
            for (int gx = -gridRadius;
                 gx <= gridRadius;
                 ++gx)
            {
                const ofVec2f offset(
                    gx * step,
                    gy * step
                );


                if (
                    offset.length() >
                    digSiteRadius
                   )
                {
                    continue;
                }


                const ofVec2f samplePosition =
                    site.position + offset;


                // The button strip remains protected even if a
                // future site-placement rule changes.
                if (
                    isInsideButtonNoDigZone(
                        samplePosition
                    )
                   )
                {
                    continue;
                }


                site.sampleOffsets.push_back(
                    offset
                );
            }
        }


        site.referenceElevations.resize(
            site.sampleOffsets.size(),
            0.0f
        );

        site.excavationDepths.resize(
            site.sampleOffsets.size(),
            0.0f
        );


        // Random buried discovery.
        //
        // 0 = dinosaur
        // 1 = treasure chest
        site.buriedObjectType =
            (ofRandom(1.0f) < 0.5f) ? 0 : 1;


        // ----------------------------------------------------
        // INDIVIDUAL SITE ORIENTATION
        //
        // Assign rotation now, when THIS site is born.
        //
        // Also try to keep it visibly different from the
        // rotations of already-created sites.
        // ----------------------------------------------------

        for (int rotationAttempt = 0;
             rotationAttempt < 20;
             ++rotationAttempt)
        {
            const float candidateRotation =
                ofRandom(0.0f, 360.0f);

            bool sufficientlyDifferent = true;

            for (const auto& existing :
                 digSites)
            {
                float difference =
                    std::abs(
                        candidateRotation -
                        existing.objectRotationDeg
                    );

                difference =
                    std::min(
                        difference,
                        360.0f - difference
                    );

                if (difference < 30.0f)
                {
                    sufficientlyDifferent = false;
                    break;
                }
            }

            site.objectRotationDeg =
                candidateRotation;

            if (sufficientlyDifferent)
                break;
        }


        digSites.push_back(site);


        std::cout
            << "[DIG] site "
            << digSites.size()
            << " contains "
            << (site.buriedObjectType == 0
                    ? "T-REX"
                    : "TREASURE CHEST")
            << " rotation="
            << site.objectRotationDeg
            << " deg"
            << std::endl;
    }


    std::cout
        << "[DIG] created "
        << digSites.size()
        << " excavation sites"
        << std::endl;

    std::cout
        << "[DIG] waiting 5 seconds before terrain reference"
        << std::endl;
}




// ============================================================
// Replace ONE completed excavation site.
//
// Placement rules:
//   - stay inside ROI
//   - stay off control buttons
//   - center on land
//   - at least 80% of circle on land
//   - stay away from all other active sites
//   - stay well away from the site just completed
//
// The new site's reference is frozen immediately because the
// child is still standing at the old completed excavation.
// ============================================================

bool ofApp::respawnDigSite(size_t siteIndex)
{
    if (!kinectProjector ||
        siteIndex >= digSites.size())
    {
        return false;
    }


    const ofVec2f oldPosition =
        digSites[siteIndex].position;

    const ofRectangle roi =
        kinectProjector->getKinectROI();


    if (roi.width < 100.0f ||
        roi.height < 100.0f)
    {
        return false;
    }


    DigSiteState replacement;

    bool accepted = false;


    // Try generously before giving up.
    // Prefer a distant new site first, then progressively
    // relax the old-site spacing if the sandbox is tight.
    for (int attempt = 0;
         attempt < 1200 && !accepted;
         ++attempt)
    {
        float oldSiteSpacingMultiplier =
            3.0f;

        if (attempt >= 900)
        {
            oldSiteSpacingMultiplier =
                1.5f;
        }
        else if (attempt >= 600)
        {
            oldSiteSpacingMultiplier =
                2.0f;
        }
        else if (attempt >= 300)
        {
            oldSiteSpacingMultiplier =
                2.5f;
        }
        replacement.position.set(
            ofRandom(
                roi.x +
                    digSiteRadius +
                    10.0f,

                roi.x +
                    roi.width -
                    digSiteRadius -
                    10.0f
            ),

            ofRandom(
                roi.y +
                    digSiteRadius +
                    10.0f,

                roi.y +
                    roi.height -
                    digSiteRadius -
                    10.0f
            )
        );


        accepted = true;


        // ----------------------------------------------------
        // Keep the replacement noticeably away from the
        // excavation the child just completed.
        // ----------------------------------------------------

        if (
            replacement.position.distance(
                oldPosition
            ) <
            digSiteRadius *
                oldSiteSpacingMultiplier
           )
        {
            accepted = false;
            continue;
        }


        // ----------------------------------------------------
        // Keep away from projected control buttons.
        // ----------------------------------------------------

        const float buttonProtection =
            digSiteRadius +
            ringButtonRadius * 2.5f;

        for (int b = 0;
             b < 4;
             ++b)
        {
        if(!MagicSandModeButtons::shown(b))continue; // MODE_BUTTONS_V37

            if (
                replacement.position.distance(
                    ringButtons[b].kinectCenter
                ) <
                buttonProtection
               )
            {
                accepted = false;
                break;
            }
        }

        if (!accepted)
            continue;


        // ----------------------------------------------------
        // LAND-ONLY TEST
        // ----------------------------------------------------

        const float shorelineBuffer =
            3.0f;

        const float centerElevation =
            kinectProjector->
                elevationAtKinectCoord(
                    replacement.position.x,
                    replacement.position.y
                );

        if (
            centerElevation <=
            shorelineBuffer
           )
        {
            accepted = false;
            continue;
        }


        int landSamples = 1;
        int totalSamples = 1;

        const int angleSamples =
            16;

        const float testRadii[3] =
        {
            digSiteRadius * 0.35f,
            digSiteRadius * 0.65f,
            digSiteRadius * 0.95f
        };


        for (float radius :
             testRadii)
        {
            for (int a = 0;
                 a < angleSamples;
                 ++a)
            {
                const float angle =
                    TWO_PI *
                    static_cast<float>(a) /
                    static_cast<float>(
                        angleSamples
                    );

                const ofVec2f testPosition(
                    replacement.position.x +
                        std::cos(angle) *
                        radius,

                    replacement.position.y +
                        std::sin(angle) *
                        radius
                );


                ++totalSamples;


                if (!roi.inside(testPosition))
                    continue;


                const float elevation =
                    kinectProjector->
                        elevationAtKinectCoord(
                            testPosition.x,
                            testPosition.y
                        );


                if (
                    elevation >
                    shorelineBuffer
                   )
                {
                    ++landSamples;
                }
            }
        }


        const float landFraction =
            static_cast<float>(
                landSamples
            ) /
            static_cast<float>(
                totalSamples
            );


        if (landFraction < 0.80f)
        {
            accepted = false;
            continue;
        }


        // ----------------------------------------------------
        // Keep away from every OTHER active dig site.
        // ----------------------------------------------------

        for (size_t i = 0;
             i < digSites.size();
             ++i)
        {
            if (i == siteIndex)
                continue;


            if (
                replacement.position.distance(
                    digSites[i].position
                ) <
                digSiteRadius * 2.25f
               )
            {
                accepted = false;
                break;
            }
        }
    }


    if (!accepted)
    {
        std::cout
            << "[DIG] could not yet find safe "
            << "replacement position for site "
            << (siteIndex + 1)
            << std::endl;

        return false;
    }


    // ========================================================
    // BUILD THE SAME 41x41 DIAGNOSTIC GRID
    // ========================================================

    const int gridRadius =
        20;

    const float step =
        digSiteRadius /
        static_cast<float>(
            gridRadius
        );


    for (int gy = -gridRadius;
         gy <= gridRadius;
         ++gy)
    {
        for (int gx = -gridRadius;
             gx <= gridRadius;
             ++gx)
        {
            const ofVec2f offset(
                gx * step,
                gy * step
            );


            if (
                offset.length() >
                digSiteRadius
               )
            {
                continue;
            }


            const ofVec2f samplePosition =
                replacement.position +
                offset;


            if (
                isInsideButtonNoDigZone(
                    samplePosition
                )
               )
            {
                continue;
            }


            replacement.sampleOffsets.push_back(
                offset
            );
        }
    }


    replacement.referenceElevations.resize(
        replacement.sampleOffsets.size(),
        0.0f
    );

    replacement.excavationDepths.resize(
        replacement.sampleOffsets.size(),
        0.0f
    );


    // ========================================================
    // RANDOM OBJECT + INDIVIDUAL RANDOM ORIENTATION
    // ========================================================

    replacement.buriedObjectType =
        (ofRandom(1.0f) < 0.5f)
            ? 0
            : 1;


    for (int rotationAttempt = 0;
         rotationAttempt < 20;
         ++rotationAttempt)
    {
        const float candidateRotation =
            ofRandom(
                0.0f,
                360.0f
            );

        bool sufficientlyDifferent =
            true;


        for (size_t i = 0;
             i < digSites.size();
             ++i)
        {
            if (i == siteIndex)
                continue;


            float difference =
                std::abs(
                    candidateRotation -
                    digSites[i]
                        .objectRotationDeg
                );

            difference =
                std::min(
                    difference,
                    360.0f -
                    difference
                );


            if (difference < 30.0f)
            {
                sufficientlyDifferent =
                    false;

                break;
            }
        }


        replacement.objectRotationDeg =
            candidateRotation;


        if (sufficientlyDifferent)
            break;
    }


    // ========================================================
    // FREEZE THE NEW SITE'S STARTING SURFACE
    // ========================================================

    float lowestReference =
        1.0e9f;


    for (size_t i = 0;
         i <
            replacement
                .sampleOffsets
                .size();
         ++i)
    {
        const ofVec2f samplePosition =
            replacement.position +
            replacement.sampleOffsets[i];


        const float elevation =
            kinectProjector->
                elevationAtKinectCoord(
                    samplePosition.x,
                    samplePosition.y
                );


        replacement
            .referenceElevations[i] =
            elevation;


        lowestReference =
            std::min(
                lowestReference,
                elevation
            );
    }


    if (
        replacement
            .referenceElevations
            .empty()
       )
    {
        return false;
    }


    replacement.objectPlaneElevation =
        lowestReference -
        20.0f;

    replacement.objectPlacementReady =
        true;


    // ========================================================
    // DENSE FROZEN SURFACE FOR THE 5 mm GEOLOGY LAYER
    // ========================================================

    replacement.denseDigRadius =
        static_cast<int>(
            std::ceil(
                digSiteRadius
            )
        );

    replacement.denseDigDiameter =
        replacement.denseDigRadius *
        2 +
        1;


    const int denseCount =
        replacement.denseDigDiameter *
        replacement.denseDigDiameter;


    replacement
        .denseDigReferenceElevations
        .assign(
            denseCount,
            1.0e9f
        );


    for (
        int dy =
            -replacement.denseDigRadius;

        dy <=
            replacement.denseDigRadius;

        ++dy
        )
    {
        for (
            int dx =
                -replacement.denseDigRadius;

            dx <=
                replacement.denseDigRadius;

            ++dx
            )
        {
            const float distanceSquared =
                static_cast<float>(
                    dx * dx +
                    dy * dy
                );


            if (
                distanceSquared >
                digSiteRadius *
                digSiteRadius
               )
            {
                continue;
            }


            const int ix =
                dx +
                replacement.denseDigRadius;

            const int iy =
                dy +
                replacement.denseDigRadius;

            const int index =
                iy *
                replacement
                    .denseDigDiameter +
                ix;


            replacement
                .denseDigReferenceElevations[
                    index
                ] =
                kinectProjector->
                    elevationAtKinectCoord(
                        replacement.position.x +
                            static_cast<float>(
                                dx
                            ),

                        replacement.position.y +
                            static_cast<float>(
                                dy
                            )
                    );
        }
    }


    replacement.referenceReady =
        true;

    replacement.excavationProgress =
        0.0f;

    replacement.revealMaskMesh.clear();

    replacement.digSandMaskMesh.clear();

    replacement.lastRevealMaskUpdate =
        -1000.0f;

    replacement.lastDigSandMaskUpdate =
        -1000.0f;


    // Completion state naturally starts over because this is a
    // new DigSiteState.
    replacement.completionLatched =
        false;

    replacement.completionTime =
        -1.0f;


    // Replace ONLY the site that was completed.
    digSites[siteIndex] =
        replacement;


    std::cout
        << "[DIG] replacement site "
        << (siteIndex + 1)
        << " contains "
        << (replacement.buriedObjectType == 0
                ? "T-REX"
                : "TREASURE CHEST")
        << " rotation="
        << replacement.objectRotationDeg
        << " deg"
        << " oldDistance="
        << replacement.position.distance(
               oldPosition
           )
        << std::endl;


    return true;
}


// ------------------------------------------------------------
// Capture the terrain reference.
//
// This is the "frozen excavation mesh." It is not the rendered
// terrain. The normal sandbox terrain continues updating.
//
// Each dig site stores what the sand looked like at activation.
// ------------------------------------------------------------

void ofApp::freezeDigSiteReference()
{
    if (!kinectProjector ||
        !digSiteModeEnabled)
    {
        return;
    }


    for (auto& site :
         digSites)
    {
        if (
            site.sampleOffsets.empty()
           )
        {
            continue;
        }


        for (size_t i = 0;
             i < site.sampleOffsets.size();
             ++i)
        {
            const ofVec2f samplePosition =
                site.position +
                site.sampleOffsets[i];


            site.referenceElevations[i] =
                kinectProjector->
                    elevationAtKinectCoord(
                        samplePosition.x,
                        samplePosition.y
                    );
        }


        // ====================================================
        // BURIED OBJECT PLACEMENT
        //
        // The object lies on ONE horizontal archaeological
        // plane underneath the starting sand.
        //
        // Because the real sand is sloped, different parts of
        // the rotated image naturally have different amounts
        // of sand above them.
        // ====================================================

        if (!site.referenceElevations.empty())
        {
            float lowestReference =
                site.referenceElevations.front();

            for (float elevation :
                 site.referenceElevations)
            {
                lowestReference =
                    std::min(
                        lowestReference,
                        elevation
                    );
            }

            // Keep the entire artifact initially buried.
            site.objectPlaneElevation =
                lowestReference - 20.0f;

            // Orientation was assigned when this
            // individual dig site was created.

            site.objectPlacementReady = true;

            // ================================================
            // DENSE FROZEN DIG SURFACE
            //
            // Save the starting sand height at roughly every
            // Kinect pixel in the excavation circle.
            //
            // Later:
            //
            //   frozen - live <= 5 mm
            //       keep normal surface terrain
            //
            //   frozen - live > 5 mm
            //       force the normal sandy-grass terrain
            // ================================================

            site.denseDigRadius =
                static_cast<int>(
                    std::ceil(digSiteRadius)
                );

            site.denseDigDiameter =
                site.denseDigRadius * 2 + 1;

            const int denseCount =
                site.denseDigDiameter *
                site.denseDigDiameter;

            site.denseDigReferenceElevations.assign(
                denseCount,
                1.0e9f
            );

            for (
                int dy = -site.denseDigRadius;
                dy <= site.denseDigRadius;
                ++dy
                )
            {
                for (
                    int dx = -site.denseDigRadius;
                    dx <= site.denseDigRadius;
                    ++dx
                    )
                {
                    const float distanceSquared =
                        static_cast<float>(
                            dx * dx +
                            dy * dy
                        );

                    if (
                        distanceSquared >
                        digSiteRadius *
                        digSiteRadius
                       )
                    {
                        continue;
                    }

                    const int ix =
                        dx +
                        site.denseDigRadius;

                    const int iy =
                        dy +
                        site.denseDigRadius;

                    const int index =
                        iy *
                        site.denseDigDiameter +
                        ix;

                    site.denseDigReferenceElevations[
                        index
                    ] =
                        kinectProjector->
                            elevationAtKinectCoord(
                                site.position.x +
                                    static_cast<float>(dx),

                                site.position.y +
                                    static_cast<float>(dy)
                            );
                }
            }

            site.digSandMaskMesh.clear();
            site.lastDigSandMaskUpdate =
                -1000.0f;

            site.revealMaskMesh.clear();
            site.lastRevealMaskUpdate =
                -1000.0f;

            std::cout
                << "[DIG OBJECT] plane="
                << site.objectPlaneElevation
                << " rotation="
                << site.objectRotationDeg
                << " deg"
                << std::endl;
        }

        site.referenceReady = true;
        site.excavationProgress = 0.0f;

        std::fill(
            site.excavationDepths.begin(),
            site.excavationDepths.end(),
            0.0f
        );
    }


    digReferenceFrozen = true;


    std::cout
        << "[DIG] excavation reference frozen"
        << std::endl;
}



// ------------------------------------------------------------
// Track excavation.
//
// HIGHER readings do not count as digging.
// This naturally ignores most hands and arms.
//
// Only terrain LOWER than the frozen reference contributes.
//
// Update at ~5 Hz to keep it extremely cheap.
// ------------------------------------------------------------

void ofApp::updateDigSites()
{
    if (!digSiteModeEnabled ||
        !kinectProjector)
    {
        return;
    }


    // --------------------------------------------------------
    // Five-second arming delay
    // --------------------------------------------------------

    if (!digReferenceFrozen)
    {
        const float elapsed =
            ofGetElapsedTimef() -
            digModeActivatedTime;


        if (elapsed >= 5.0f)
        {
            freezeDigSiteReference();
        }


        return;
    }


    // --------------------------------------------------------
    // Excavation tracking cadence: about five times/sec
    // --------------------------------------------------------

    static float lastDigUpdate = -1000.0f;

    const float now =
        ofGetElapsedTimef();


    if (
        now - lastDigUpdate <
        0.20f
       )
    {
        return;
    }


    lastDigUpdate = now;


    // ========================================================
    // COMPLETION LIFE CYCLE
    //
    //   0-10 sec after completion:
    //       leave artifact visible
    //
    //   10-13 sec:
    //       GOOD JOB is drawn by drawDigSites()
    //
    //   13+ sec:
    //       replace only this completed site
    // ========================================================

    for (size_t siteIndex = 0;
         siteIndex < digSites.size();
         ++siteIndex)
    {
        const auto& site =
            digSites[siteIndex];


        if (
            !site.completionLatched ||
            site.completionTime < 0.0f
           )
        {
            continue;
        }


        const float completionAge =
            now -
            site.completionTime;


        if (completionAge >= 13.0f)
        {
            respawnDigSite(
                siteIndex
            );
        }
    }



    // The sandbox has about four inches (~102 mm) of usable
    // depth before water level.
    //
    // Do NOT require anywhere near that much excavation.
    //
    // For this diagnostic progress meter, 40 mm (~1.6 inches)
    // at a sample counts as fully excavated.
    const float fullSampleExcavationDepth =
        40.0f;


    for (auto& site :
         digSites)
    {
        if (
            !site.referenceReady ||
            site.sampleOffsets.empty()
           )
        {
            continue;
        }


        float accumulatedProgress = 0.0f;


        for (size_t i = 0;
             i < site.sampleOffsets.size();
             ++i)
        {
            const ofVec2f samplePosition =
                site.position +
                site.sampleOffsets[i];


            if (
                isInsideButtonNoDigZone(
                    samplePosition
                )
               )
            {
                continue;
            }


            const float liveElevation =
                kinectProjector->
                    elevationAtKinectCoord(
                        samplePosition.x,
                        samplePosition.y
                    );


            // Positive means sand was removed.
            //
            // Hand/arm above the sand creates a negative number
            // and contributes zero.
            const float excavatedDepth =
                site.referenceElevations[i] -
                liveElevation;


            // Higher objects such as hands create negative
            // excavation and therefore count as zero.
            site.excavationDepths[i] =
                std::max(
                    0.0f,
                    excavatedDepth
                );


            const float sampleProgress =
                ofClamp(
                    site.excavationDepths[i] /
                        fullSampleExcavationDepth,
                    0.0f,
                    1.0f
                );


            accumulatedProgress +=
                sampleProgress;
        }


        site.excavationProgress =
            accumulatedProgress /
            static_cast<float>(
                site.sampleOffsets.size()
            );
    }
}



// ------------------------------------------------------------
// Draw excavation areas.
//
// Thick boundary so it remains visible against terrain.
// ------------------------------------------------------------

void ofApp::drawDigSites()
{
    if (!kinectProjector ||
        !digSiteModeEnabled)
    {
        return;
    }


    const float elapsed =
        ofGetElapsedTimef() -
        digModeActivatedTime;


    const float remaining =
        std::max(
            0.0f,
            5.0f - elapsed
        );


    ofPushStyle();


    for (auto& site :
         digSites)
    {
        const ofVec2f center =
            kinectProjector->
                kinectCoordToProjCoord(
                    site.position.x,
                    site.position.y
                );


        const ofVec2f radiusXPoint =
            kinectProjector->
                kinectCoordToProjCoord(
                    site.position.x +
                        digSiteRadius,
                    site.position.y
                );


        const ofVec2f radiusYPoint =
            kinectProjector->
                kinectCoordToProjCoord(
                    site.position.x,
                    site.position.y +
                        digSiteRadius
                );


        const float radiusX =
            std::max(
                10.0f,
                (radiusXPoint - center).length()
            );


        const float radiusY =
            std::max(
                10.0f,
                (radiusYPoint - center).length()
            );


        // ====================================================
        // EXPANDING ROCK EXCAVATION RIM
        //
        // Once actual excavation exceeds 5 mm:
        //
        //   1. The red target disappears.
        //   2. A small rock ring begins at the center X.
        //   3. It expands smoothly for 3 seconds.
        //   4. Its INNER edge finishes exactly at the original
        //      red-circle boundary.
        //   5. The rock thickness extends OUTWARD from there.
        //
        // Rim thickness is proportional to the ring's CURRENT
        // radius, so the rim starts thin and becomes thicker as
        // the hole grows.
        // ====================================================

        bool diggingStartedForRim =
            site.rockRimStartTime >= 0.0f;


        if (!diggingStartedForRim &&
            site.referenceReady)
        {
            for (float depth :
                 site.excavationDepths)
            {
                if (depth > 5.0f)
                {
                    site.rockRimStartTime =
                        ofGetElapsedTimef();

                    diggingStartedForRim =
                        true;

                    std::cout
                        << "[DIG ROCK RIM] expansion started"
                        << std::endl;

                    break;
                }
            }
        }


        // If a completed site somehow reached completion without
        // latching the animation, force it to the fully-expanded
        // state rather than showing no rim.
        if (site.completionLatched &&
            site.rockRimStartTime < 0.0f)
        {
            site.rockRimStartTime =
                ofGetElapsedTimef() -
                3.0f;

            diggingStartedForRim =
                true;
        }


        bool showRockRim =
            diggingStartedForRim;


        // Keep the rim through:
        //
        //   0-10 sec  artifact admiration
        //   10-13 sec GOOD JOB
        //
        // Then remove it with the completed site.
        if (site.completionLatched &&
            site.completionTime >= 0.0f)
        {
            const float completionAge =
                ofGetElapsedTimef() -
                site.completionTime;

            if (completionAge >= 13.0f)
            {
                showRockRim =
                    false;
            }
        }


        if (showRockRim)
        {
            static ofImage rockRimImage;

            static bool rockRimImageTried =
                false;

            static bool rockRimImageLoaded =
                false;


            if (!rockRimImageTried)
            {
                rockRimImageTried =
                    true;

                rockRimImageLoaded =
                    rockRimImage.load(
                        "textures/rock_diff.jpg"
                    );

                std::cout
                    << "[DIG ROCK RIM] texture "
                    << (
                        rockRimImageLoaded
                            ? "loaded"
                            : "FAILED"
                       )
                    << std::endl;
            }


            if (rockRimImageLoaded)
            {
                // --------------------------------------------
                // THREE-SECOND EXPANSION
                // --------------------------------------------

                const float animationAge =
                    ofGetElapsedTimef() -
                    site.rockRimStartTime;


                float t =
                    ofClamp(
                        animationAge /
                            3.0f,
                        0.0f,
                        1.0f
                    );


                // Smoothstep:
                // slow departure, visible movement through the
                // middle, and gentle arrival at full size.
                t =
                    t *
                    t *
                    (
                        3.0f -
                        2.0f * t
                    );


                // Start at 6% of the target radius.
                //
                // At t = 1.0:
                //
                // innerRadiusX == radiusX
                // innerRadiusY == radiusY
                //
                // Therefore the inner edge lands exactly on the
                // original red-circle boundary.
                const float ringScale =
                    0.06f +
                    0.94f * t;


                const float innerRadiusX =
                    radiusX *
                    ringScale;

                const float innerRadiusY =
                    radiusY *
                    ringScale;


                // --------------------------------------------
                // RIM THICKNESS FOLLOWS ITS OWN CURRENT RADIUS
                //
                // This is the change you suggested.
                //
                // Tiny ring  -> thin rock lip
                // Large ring -> thick rock lip
                //
                // 42% gives roughly the visual thickness we
                // were aiming for relative to GOOD JOB.
                // --------------------------------------------

                const float currentRingRadius =
                    std::min(
                        innerRadiusX,
                        innerRadiusY
                    );


                const float rimThickness =
                    currentRingRadius *
                    0.42f;


                // Entire thickness grows OUTWARD.
                const float outerRadiusX =
                    innerRadiusX +
                    rimThickness;

                const float outerRadiusY =
                    innerRadiusY +
                    rimThickness;


                // --------------------------------------------
                // BUILD TEXTURED ANNULUS
                // --------------------------------------------

                ofMesh rockRingMesh;

                rockRingMesh.setMode(
                    OF_PRIMITIVE_TRIANGLE_STRIP
                );


                const int ringSegments =
                    128;


                const float textureW =
                    static_cast<float>(
                        rockRimImage.getWidth()
                    );

                const float textureH =
                    static_cast<float>(
                        rockRimImage.getHeight()
                    );


                for (int segment = 0;
                     segment <= ringSegments;
                     ++segment)
                {
                    const float fraction =
                        static_cast<float>(
                            segment
                        ) /
                        static_cast<float>(
                            ringSegments
                        );


                    const float angle =
                        fraction *
                        TWO_PI;


                    const float cs =
                        std::cos(angle);

                    const float sn =
                        std::sin(angle);


                    // OUTER EDGE
                    rockRingMesh.addVertex(
                        ofVec3f(
                            center.x +
                                cs *
                                outerRadiusX,

                            center.y +
                                sn *
                                outerRadiusY,

                            0.0f
                        )
                    );


                    rockRingMesh.addTexCoord(
                        ofVec2f(
                            (
                                cs *
                                0.50f +
                                0.50f
                            ) *
                                textureW,

                            (
                                sn *
                                0.50f +
                                0.50f
                            ) *
                                textureH
                        )
                    );


                    // INNER EDGE
                    rockRingMesh.addVertex(
                        ofVec3f(
                            center.x +
                                cs *
                                innerRadiusX,

                            center.y +
                                sn *
                                innerRadiusY,

                            0.0f
                        )
                    );


                    rockRingMesh.addTexCoord(
                        ofVec2f(
                            (
                                cs *
                                0.30f +
                                0.50f
                            ) *
                                textureW,

                            (
                                sn *
                                0.30f +
                                0.50f
                            ) *
                                textureH
                        )
                    );
                }


                // --------------------------------------------
                // DRAW AS TRUE PROJECTOR OVERLAY
                //
                // The live terrain uses depth/stencil state.
                // The rock lip is a projected visual marker and
                // must remain visible regardless of sand depth.
                // --------------------------------------------

                const GLboolean depthWasEnabled =
                    glIsEnabled(
                        GL_DEPTH_TEST
                    );

                const GLboolean stencilWasEnabled =
                    glIsEnabled(
                        GL_STENCIL_TEST
                    );

                const GLboolean cullWasEnabled =
                    glIsEnabled(
                        GL_CULL_FACE
                    );


                glDisable(
                    GL_DEPTH_TEST
                );

                glDisable(
                    GL_STENCIL_TEST
                );

                glDisable(
                    GL_CULL_FACE
                );


                ofPushStyle();

                ofEnableAlphaBlending();

                ofFill();

                ofSetColor(
                    255,
                    255,
                    255,
                    255
                );


                rockRimImage
                    .getTexture()
                    .bind();


                rockRingMesh.draw();


                rockRimImage
                    .getTexture()
                    .unbind();


                ofPopStyle();


                // Restore exactly the enable/disable states that
                // existed before drawing the ring.
                if (depthWasEnabled)
                {
                    glEnable(
                        GL_DEPTH_TEST
                    );
                }
                else
                {
                    glDisable(
                        GL_DEPTH_TEST
                    );
                }


                if (stencilWasEnabled)
                {
                    glEnable(
                        GL_STENCIL_TEST
                    );
                }
                else
                {
                    glDisable(
                        GL_STENCIL_TEST
                    );
                }


                if (cullWasEnabled)
                {
                    glEnable(
                        GL_CULL_FACE
                    );
                }
                else
                {
                    glDisable(
                        GL_CULL_FACE
                    );
                }
            }
        }


        // ====================================================
        // COMPLETED SITE MESSAGE
        //
        // First 10 seconds:
        //     nothing special here; the fully uncovered object
        //     continues drawing normally so the child can admire it.
        //
        // Next 3 seconds:
        //     close the excavation visually and echo GOOD JOB!
        //     in the same projected circle.
        // ====================================================

        if (
            site.completionLatched &&
            site.completionTime >= 0.0f
           )
        {
            const float completionAge =
                ofGetElapsedTimef() -
                site.completionTime;


            // After 13 seconds, hide the completed site completely.
            // updateDigSites() will continue searching for a valid
            // replacement position in the background.
            if (completionAge >= 13.0f)
            {
                continue;
            }

            if (completionAge >= 10.0f)
            {
                const float messageTime =
                    completionAge -
                    10.0f;


                // GOOD JOB remains visible while respawn searches
                // for a valid new location. Normally this lasts
                // exactly three seconds.
                const float radius =
                    std::min(
                        radiusX,
                        radiusY
                    );


                const float baseScale =
                    ofClamp(
                        radius / 58.0f,
                        0.85f,
                        2.0f
                    );


                // ====================================================
                // LARGE SMOOTH GOOD JOB MESSAGE
                // ====================================================

                const std::string line1 = "GOOD";
                const std::string line2 = "JOB!";

                static ofTrueTypeFont goodJobFont;
                static bool goodJobFontInitialized = false;
                static bool goodJobFontLoaded = false;

                if (!goodJobFontInitialized)
                {
                    goodJobFontInitialized = true;

                    goodJobFontLoaded =
                        goodJobFont.load(
                            "Verdana.ttf",
                            72,
                            true,
                            true,
                            true
                        );

                    std::cout
                        << "[DIG GOOD JOB FONT] "
                        << (goodJobFontLoaded ? "loaded" : "FAILED")
                        << std::endl;
                }

                glDisable(GL_STENCIL_TEST);

                glColorMask(
                    GL_TRUE,
                    GL_TRUE,
                    GL_TRUE,
                    GL_TRUE
                );

                ofEnableAlphaBlending();

                if (goodJobFontLoaded)
                {
                    const ofRectangle box1 =
                        goodJobFont.getStringBoundingBox(
                            line1,
                            0.0f,
                            0.0f
                        );

                    const ofRectangle box2 =
                        goodJobFont.getStringBoundingBox(
                            line2,
                            0.0f,
                            0.0f
                        );

                    const float widest =
                        std::max(
                            box1.width,
                            box2.width
                        );

                    const float unscaledHeight =
                        box1.height +
                        box2.height +
                        10.0f;

                    // Fill nearly the whole excavation circle.
                    const float targetWidth =
                        radiusX *
                        2.0f *
                        0.94f;

                    const float targetHeight =
                        radiusY *
                        2.0f *
                        0.90f;

                    const float scaleX =
                        targetWidth /
                        std::max(
                            widest,
                            1.0f
                        );

                    const float scaleY =
                        targetHeight /
                        std::max(
                            unscaledHeight,
                            1.0f
                        );

                    const float textScale =
                        std::min(
                            scaleX,
                            scaleY
                        );

                    const float lineGap =
                        10.0f;

                    const float totalHeight =
                        box1.height +
                        lineGap +
                        box2.height;

                    const float firstBaseline =
                        -totalHeight * 0.5f -
                        box1.y;

                    const float secondBaseline =
                        firstBaseline +
                        box1.height +
                        lineGap -
                        box2.y;

                    ofPushMatrix();

                    ofTranslate(
                        center.x,
                        center.y
                    );

                    ofScale(
                        textScale,
                        textScale
                    );

                    // Thick white outline so it stays readable
                    // on every terrain color.
                    ofSetColor(
                        255,
                        255,
                        255,
                        255
                    );

                    for (int oy = -3; oy <= 3; ++oy)
                    {
                        for (int ox = -3; ox <= 3; ++ox)
                        {
                            if (
                                ox == 0 &&
                                oy == 0
                               )
                            {
                                continue;
                            }

                            goodJobFont.drawString(
                                line1,
                                -box1.width * 0.5f +
                                    static_cast<float>(ox),
                                firstBaseline +
                                    static_cast<float>(oy)
                            );

                            goodJobFont.drawString(
                                line2,
                                -box2.width * 0.5f +
                                    static_cast<float>(ox),
                                secondBaseline +
                                    static_cast<float>(oy)
                            );
                        }
                    }

                    // Solid black center.
                    ofSetColor(
                        0,
                        0,
                        0,
                        255
                    );

                    goodJobFont.drawString(
                        line1,
                        -box1.width * 0.5f,
                        firstBaseline
                    );

                    goodJobFont.drawString(
                        line2,
                        -box2.width * 0.5f,
                        secondBaseline
                    );

                    ofPopMatrix();

                    static float lastGoodJobLog =
                        -1000.0f;

                    const float now =
                        ofGetElapsedTimef();

                    if (
                        now -
                        lastGoodJobLog >=
                        1.0f
                       )
                    {
                        lastGoodJobLog =
                            now;

                        std::cout
                            << "[DIG GOOD JOB TTF] circle="
                            << radiusX * 2.0f
                            << "x"
                            << radiusY * 2.0f
                            << " scale="
                            << textScale
                            << std::endl;
                    }
                }

                // Do not draw fossil/chest/geology during
                // the GOOD JOB phase.
                continue;
            }
        }


        // ====================================================
        // SIMPLE DYNAMIC DIG REVEAL
        //
        // Every sample uses its OWN frozen starting elevation.
        //
        // 0 - 5 mm:
        //     leave live terrain completely alone
        //
        // 5+ mm:
        //     show sandy excavation circles
        //
        // 20 - 70 mm:
        //     progressively reveal artifact locally
        //
        // 70+ mm:
        //     artifact fully revealed locally
        //
        // No stencil.
        // No full-site sand layer at activation.
        // No automatic terrain stripping.
        // ====================================================

        if (site.referenceReady &&
            site.excavationDepths.size() ==
                site.sampleOffsets.size())
        {
            static ofImage dinoImage;
            static ofImage chestImage;
            static ofImage digSandImage;
            static bool digImagesLoaded = false;

            if (!digImagesLoaded)
            {
                const bool dinoOK =
                    dinoImage.load(
                        "dig/dinotrex.png"
                    );

                const bool chestOK =
                    chestImage.load(
                        "dig/closedchest.png"
                    );

                const bool sandOK =
                    digSandImage.load(
                        "textures/sandygrass.jpg"
                    );

                std::cout
                    << "[DIG ASSETS] dinosaur="
                    << (dinoOK ? "OK" : "FAILED")
                    << " chest="
                    << (chestOK ? "OK" : "FAILED")
                    << " sand="
                    << (sandOK ? "OK" : "FAILED")
                    << std::endl;

                digImagesLoaded = true;
            }


            ofImage* buriedImage =
                site.buriedObjectType == 0
                    ? &dinoImage
                    : &chestImage;


            const float sampleStepKinect =
                digSiteRadius / 20.0f;


            // -------------------------------------------------
            // Determine whether excavation has REALLY started.
            // Noise below 5 mm does not count.
            // -------------------------------------------------

            bool diggingStarted = false;

            for (float depth :
                 site.excavationDepths)
            {
                if (depth > 5.0f)
                {
                    diggingStarted = true;
                    break;
                }
            }


            // -------------------------------------------------
            // PASS 1:
            // Sand circles ONLY where actual excavation is
            // deeper than 5 mm.
            //
            // This is intentionally similar to the earlier
            // color-layer test that behaved correctly.
            // -------------------------------------------------

            // Physical sand is the top covering layer.
            // Do not project artificial excavation sand.
            if (false && diggingStarted)
            {
                for (size_t i = 0;
                     i < site.sampleOffsets.size();
                     ++i)
                {
                    const float depth =
                        site.excavationDepths[i];

                    if (depth <= 5.0f)
                        continue;


                    const ofVec2f offset =
                        site.sampleOffsets[i];

                    const ofVec2f sampleKinect =
                        site.position + offset;

                    const ofVec2f sampleProj =
                        kinectProjector->
                            kinectCoordToProjCoord(
                                sampleKinect.x,
                                sampleKinect.y
                            );

                    const ofVec2f sampleProjX =
                        kinectProjector->
                            kinectCoordToProjCoord(
                                sampleKinect.x +
                                    sampleStepKinect,
                                sampleKinect.y
                            );

                    const ofVec2f sampleProjY =
                        kinectProjector->
                            kinectCoordToProjCoord(
                                sampleKinect.x,
                                sampleKinect.y +
                                    sampleStepKinect
                            );


                    const float patchRadiusX =
                        ofClamp(
                            (sampleProjX -
                             sampleProj).length() *
                                0.72f,
                            5.0f,
                            35.0f
                        );

                    const float patchRadiusY =
                        ofClamp(
                            (sampleProjY -
                             sampleProj).length() *
                                0.72f,
                            5.0f,
                            35.0f
                        );


                    // -----------------------------------------
                    // Use a color sampled from sandygrass.jpg.
                    //
                    // This preserves the simple circle behavior
                    // instead of drawing the entire texture
                    // across the site.
                    // -----------------------------------------

                    ofColor sandColor(
                        205,
                        180,
                        120
                    );

                    if (digSandImage.isAllocated())
                    {
                        const ofPixels& px =
                            digSandImage.getPixels();

                        if (px.isAllocated() &&
                            px.getWidth() > 0 &&
                            px.getHeight() > 0)
                        {
                            const int tx =
                                (
                                    static_cast<int>(
                                        std::abs(
                                            sampleProj.x *
                                            0.35f
                                        )
                                    )
                                ) %
                                px.getWidth();

                            const int ty =
                                (
                                    static_cast<int>(
                                        std::abs(
                                            sampleProj.y *
                                            0.35f
                                        )
                                    )
                                ) %
                                px.getHeight();

                            sandColor =
                                px.getColor(
                                    tx,
                                    ty
                                );
                        }
                    }


                    // -----------------------------------------
                    // Smooth alpha-mask excavation stamp.
                    //
                    // Instead of drawing visible circles, each
                    // Kinect sample places a tiny radial alpha
                    // gradient. Neighboring samples overlap and
                    // blend into one continuous excavation mask.
                    // -----------------------------------------

                    static ofImage digAlphaStamp;
                    static bool digAlphaStampReady = false;


                    if (!digAlphaStampReady)
                    {
                        const int stampSize = 32;

                        ofPixels stampPixels;

                        stampPixels.allocate(
                            stampSize,
                            stampSize,
                            OF_PIXELS_RGBA
                        );


                        for (int sy = 0;
                             sy < stampSize;
                             ++sy)
                        {
                            for (int sx = 0;
                                 sx < stampSize;
                                 ++sx)
                            {
                                const float nx =
                                    (
                                        static_cast<float>(sx) +
                                        0.5f
                                    ) /
                                    static_cast<float>(stampSize) *
                                    2.0f - 1.0f;


                                const float ny =
                                    (
                                        static_cast<float>(sy) +
                                        0.5f
                                    ) /
                                    static_cast<float>(stampSize) *
                                    2.0f - 1.0f;


                                const float r =
                                    std::sqrt(
                                        nx * nx +
                                        ny * ny
                                    );


                                float alpha = 0.0f;


                                // Solid-ish center, then a smooth
                                // falloff instead of a hard circle.
                                if (r <= 0.58f)
                                {
                                    alpha = 1.0f;
                                }
                                else if (r < 1.0f)
                                {
                                    float t =
                                        (1.0f - r) /
                                        (1.0f - 0.58f);

                                    t =
                                        ofClamp(
                                            t,
                                            0.0f,
                                            1.0f
                                        );

                                    // smoothstep
                                    alpha =
                                        t * t *
                                        (3.0f - 2.0f * t);
                                }


                                stampPixels.setColor(
                                    sx,
                                    sy,
                                    ofColor(
                                        255,
                                        255,
                                        255,
                                        static_cast<unsigned char>(
                                            alpha * 255.0f
                                        )
                                    )
                                );
                            }
                        }


                        digAlphaStamp.setFromPixels(
                            stampPixels
                        );

                        digAlphaStampReady = true;
                    }


                    ofEnableAlphaBlending();

                    ofSetColor(
                        sandColor,
                        255
                    );


                    // Slight overlap between neighboring
                    // one-Kinect-pixel samples prevents gaps.
                    const float stampWidth =
                        patchRadiusX * 2.55f;

                    const float stampHeight =
                        patchRadiusY * 2.55f;


                    digAlphaStamp.draw(
                        sampleProj.x -
                            stampWidth * 0.5f,

                        sampleProj.y -
                            stampHeight * 0.5f,

                        stampWidth,
                        stampHeight
                    );
                }
            }



            // =================================================
            // DIG GEOLOGY OVERRIDE
            //
            // The original live terrain renderer still handles
            // the entire sandbox normally.
            //
            // Inside this DIG circle only:
            //
            //    0 - 5 mm excavation:
            //        leave original terrain visible
            //
            //    deeper than 5 mm:
            //        cover terrain with sandygrass.jpg
            //
            // The buried-object renderer below remains
            // completely independent and unchanged.
            // =================================================

            if (
                site.referenceReady &&
                site.objectPlacementReady &&
                digSandImage.isAllocated() &&
                site.denseDigDiameter > 0 &&
                !site.denseDigReferenceElevations.empty()
               )
            {
                const float sandMaskNow =
                    ofGetElapsedTimef();

                if (
                    sandMaskNow -
                    site.lastDigSandMaskUpdate >=
                        0.20f ||
                    site.digSandMaskMesh
                        .getNumVertices() == 0
                   )
                {
                    site.lastDigSandMaskUpdate =
                        sandMaskNow;

                    site.digSandMaskMesh.clear();

                    site.digSandMaskMesh.setMode(
                        OF_PRIMITIVE_TRIANGLES
                    );


                    const float halfPixel =
                        0.53f;


                    for (
                        int iy = 0;
                        iy < site.denseDigDiameter;
                        ++iy
                        )
                    {
                        for (
                            int ix = 0;
                            ix < site.denseDigDiameter;
                            ++ix
                            )
                        {
                            const int index =
                                iy *
                                site.denseDigDiameter +
                                ix;


                            if (
                                index < 0 ||
                                index >=
                                    static_cast<int>(
                                        site
                                            .denseDigReferenceElevations
                                            .size()
                                    )
                               )
                            {
                                continue;
                            }


                            const float frozenElevation =
                                site
                                    .denseDigReferenceElevations[
                                        index
                                    ];


                            // Sentinel means this pixel lies
                            // outside the circular dig site.
                            if (
                                frozenElevation >
                                1.0e8f
                               )
                            {
                                continue;
                            }


                            const float localX =
                                static_cast<float>(
                                    ix -
                                    site.denseDigRadius
                                );

                            const float localY =
                                static_cast<float>(
                                    iy -
                                    site.denseDigRadius
                                );


                            const float liveElevation =
                                kinectProjector->
                                    elevationAtKinectCoord(
                                        site.position.x +
                                            localX,

                                        site.position.y +
                                            localY
                                    );


                            const float localExcavationDepth =
                                frozenElevation -
                                liveElevation;


                            // ---------------------------------
                            // TOP GEOLOGICAL CRUST
                            //
                            // First 5 mm remains whatever the
                            // normal terrain renderer shows.
                            // ---------------------------------

                            if (
                                localExcavationDepth <=
                                5.0f
                               )
                            {
                                continue;
                            }


                            // ---------------------------------
                            // BELOW 5 MM:
                            // this Kinect pixel is now sand.
                            // ---------------------------------

                            const float kx =
                                site.position.x +
                                localX;

                            const float ky =
                                site.position.y +
                                localY;


                            const ofVec2f p0 =
                                kinectProjector->
                                    kinectCoordToProjCoord(
                                        kx - halfPixel,
                                        ky - halfPixel
                                    );

                            const ofVec2f p1 =
                                kinectProjector->
                                    kinectCoordToProjCoord(
                                        kx + halfPixel,
                                        ky - halfPixel
                                    );

                            const ofVec2f p2 =
                                kinectProjector->
                                    kinectCoordToProjCoord(
                                        kx + halfPixel,
                                        ky + halfPixel
                                    );

                            const ofVec2f p3 =
                                kinectProjector->
                                    kinectCoordToProjCoord(
                                        kx - halfPixel,
                                        ky + halfPixel
                                    );


                            // Triangle 1
                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p0.x,
                                    p0.y,
                                    0.0f
                                )
                            );

                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p1.x,
                                    p1.y,
                                    0.0f
                                )
                            );

                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p2.x,
                                    p2.y,
                                    0.0f
                                )
                            );


                            // Triangle 2
                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p0.x,
                                    p0.y,
                                    0.0f
                                )
                            );

                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p2.x,
                                    p2.y,
                                    0.0f
                                )
                            );

                            site.digSandMaskMesh.addVertex(
                                ofVec3f(
                                    p3.x,
                                    p3.y,
                                    0.0f
                                )
                            );
                        }
                    }
                }


                // ---------------------------------------------
                // Use the dense mesh only as a stencil.
                // Then tile THE SAME sandygrass.jpg used by
                // the terrain system through that stencil.
                // ---------------------------------------------

                if (
                    site.digSandMaskMesh
                        .getNumVertices() > 0
                   )
                {
                    GLint sandStencilBits = 0;

                    glGetIntegerv(
                        GL_STENCIL_BITS,
                        &sandStencilBits
                    );


                    if (sandStencilBits > 0)
                    {
                        glEnable(
                            GL_STENCIL_TEST
                        );

                        glClearStencil(0);

                        glClear(
                            GL_STENCIL_BUFFER_BIT
                        );


                        glColorMask(
                            GL_FALSE,
                            GL_FALSE,
                            GL_FALSE,
                            GL_FALSE
                        );

                        glStencilFunc(
                            GL_ALWAYS,
                            1,
                            0xFF
                        );

                        glStencilOp(
                            GL_KEEP,
                            GL_KEEP,
                            GL_REPLACE
                        );


                        site.digSandMaskMesh.draw();


                        glColorMask(
                            GL_TRUE,
                            GL_TRUE,
                            GL_TRUE,
                            GL_TRUE
                        );

                        glStencilFunc(
                            GL_EQUAL,
                            1,
                            0xFF
                        );

                        glStencilOp(
                            GL_KEEP,
                            GL_KEEP,
                            GL_KEEP
                        );


                        ofSetColor(255);


                        // Main terrain shader uses:
                        //
                        // gl_FragCoord * 4.2
                        //
                        // for sandygrass.jpg.
                        //
                        // Scaling the image to 1/4.2 of its
                        // native size produces approximately
                        // the same visible texture scale.

                        const float tileW =
                            std::max(
                                1.0f,
                                digSandImage.getWidth() /
                                    4.2f
                            );

                        const float tileH =
                            std::max(
                                1.0f,
                                digSandImage.getHeight() /
                                    4.2f
                            );


                        const float projectorW =
                            projWindow
                                ? projWindow->getWidth()
                                : 1024.0f;

                        const float projectorH =
                            projWindow
                                ? projWindow->getHeight()
                                : 768.0f;


                        for (
                            float ty = 0.0f;
                            ty < projectorH;
                            ty += tileH
                            )
                        {
                            for (
                                float tx = 0.0f;
                                tx < projectorW;
                                tx += tileW
                                )
                            {
                                digSandImage.draw(
                                    tx,
                                    ty,
                                    tileW,
                                    tileH
                                );
                            }
                        }


                        glDisable(
                            GL_STENCIL_TEST
                        );
                    }
                }
            }


            // -------------------------------------------------
            // PASS 2:
            // ONE centered artifact, revealed locally.
            //
            // The artifact itself is never tiled.
            //
            // Local excavation circles act only as a visibility
            // mask. A deeper spot exposes more of the ONE image
            // underneath it.
            //
            // There is NO whole-site progress shortcut.
            // -------------------------------------------------

            if (buriedImage &&
                buriedImage->isAllocated())
            {
                GLint stencilBits = 0;

                glGetIntegerv(
                    GL_STENCIL_BITS,
                    &stencilBits
                );


                if (stencilBits > 0)
                {
                    glEnable(
                        GL_STENCIL_TEST
                    );

                    glClearStencil(0);

                    glClear(
                        GL_STENCIL_BUFFER_BIT
                    );


                    // -----------------------------------------
                    // First build the local reveal mask.
                    // Don't draw visible colors yet.
                    // -----------------------------------------

                    glColorMask(
                        GL_FALSE,
                        GL_FALSE,
                        GL_FALSE,
                        GL_FALSE
                    );

                    glStencilFunc(
                        GL_ALWAYS,
                        1,
                        0xFF
                    );

                    glStencilOp(
                        GL_KEEP,
                        GL_KEEP,
                        GL_REPLACE
                    );


                    // =========================================
                    // DENSE ROTATED CT-SCAN REVEAL MASK
                    //
                    // This no longer uses sampleOffsets to decide
                    // which parts of the PNG are visible.
                    //
                    // Instead we cover the object's footprint at
                    // approximately ONE KINECT PIXEL resolution.
                    //
                    // Each pixel asks:
                    //
                    // live sand elevation <= buried plane ?
                    //
                    // The resulting geometry is batched into ONE
                    // mesh draw rather than thousands of circles.
                    // =========================================

                    if (site.objectPlacementReady &&
                        buriedImage &&
                        buriedImage->isAllocated())
                    {
                        const float maskNow =
                            ofGetElapsedTimef();

                        if (
                            maskNow -
                            site.lastRevealMaskUpdate >=
                                0.20f ||
                            site.revealMaskMesh
                                .getNumVertices() == 0
                           )
                        {
                            site.lastRevealMaskUpdate =
                                maskNow;

                            site.revealMaskMesh.clear();

                            // Reset TRUE visible-object counts
                            // each time the dense mask is rebuilt.
                            site.revealEligibleCells = 0;
                            site.revealVisibleCells = 0;
                            site.actualRevealFraction = 0.0f;

                            site.revealMaskMesh.setMode(
                                OF_PRIMITIVE_TRIANGLES
                            );


                            const float imageW =
                                buriedImage->getWidth();

                            const float imageH =
                                buriedImage->getHeight();


                            if (imageW > 0.0f &&
                                imageH > 0.0f)
                            {
                                const float objectScale =
                                    site.buriedObjectType == 1
                                        ? 0.40f
                                        : 0.95f;


                                const float imageAspect =
                                    imageW / imageH;


                                const float maxKinectSize =
                                    digSiteRadius *
                                    2.0f *
                                    objectScale;


                                float objectKinectW;
                                float objectKinectH;


                                if (imageAspect >= 1.0f)
                                {
                                    objectKinectW =
                                        maxKinectSize;

                                    objectKinectH =
                                        objectKinectW /
                                        imageAspect;
                                }
                                else
                                {
                                    objectKinectH =
                                        maxKinectSize;

                                    objectKinectW =
                                        objectKinectH *
                                        imageAspect;
                                }


                                const float angle =
                                    ofDegToRad(
                                        site.objectRotationDeg
                                    );


                                const float ca =
                                    std::cos(angle);

                                const float sa =
                                    std::sin(angle);


                                auto rotateLocal =
                                    [ca, sa](
                                        float x,
                                        float y
                                    )
                                    {
                                        return ofVec2f(
                                            ca * x -
                                                sa * y,

                                            sa * x +
                                                ca * y
                                        );
                                    };


                                // One Kinect pixel is already the
                                // limiting sensor resolution.
                                const float step =
                                    1.0f;


                                const float halfW =
                                    objectKinectW *
                                    0.5f;

                                const float halfH =
                                    objectKinectH *
                                    0.5f;


                                for (
                                    float localY =
                                        -halfH;

                                    localY < halfH;

                                    localY += step
                                    )
                                {
                                    for (
                                        float localX =
                                            -halfW;

                                        localX < halfW;

                                        localX += step
                                        )
                                    {
                                        const ofVec2f rotatedCenter =
                                            rotateLocal(
                                                localX +
                                                    step * 0.5f,

                                                localY +
                                                    step * 0.5f
                                            );


                                        // ---------------------------------
                                        // COUNT ONLY ACTUAL PNG CONTENT
                                        //
                                        // Transparent pixels do not count
                                        // toward "100% uncovered."
                                        // ---------------------------------

                                        const float imageU =
                                            ofClamp(
                                                (
                                                    localX +
                                                    step * 0.5f +
                                                    halfW
                                                ) /
                                                objectKinectW,
                                                0.0f,
                                                0.999999f
                                            );

                                        const float imageV =
                                            ofClamp(
                                                (
                                                    localY +
                                                    step * 0.5f +
                                                    halfH
                                                ) /
                                                objectKinectH,
                                                0.0f,
                                                0.999999f
                                            );


                                        const int imageX =
                                            ofClamp(
                                                static_cast<int>(
                                                    imageU *
                                                    imageW
                                                ),
                                                0,
                                                static_cast<int>(
                                                    imageW
                                                ) - 1
                                            );

                                        const int imageY =
                                            ofClamp(
                                                static_cast<int>(
                                                    imageV *
                                                    imageH
                                                ),
                                                0,
                                                static_cast<int>(
                                                    imageH
                                                ) - 1
                                            );


                                        const ofColor sourcePixel =
                                            buriedImage->
                                                getColor(
                                                    imageX,
                                                    imageY
                                                );


                                        // Ignore transparent and nearly
                                        // transparent antialiased edges.
                                        if (sourcePixel.a <= 24)
                                        {
                                            continue;
                                        }


                                        ++site.revealEligibleCells;


                                        const ofVec2f sampleKinect =
                                            site.position +
                                            rotatedCenter;


                                        const float liveElevation =
                                            kinectProjector->
                                                elevationAtKinectCoord(
                                                    sampleKinect.x,
                                                    sampleKinect.y
                                                );


                                        // Sand is still ABOVE
                                        // the artifact at this
                                        // location: keep hidden.
                                        if (
                                            liveElevation >
                                            site.objectPlaneElevation
                                           )
                                        {
                                            continue;
                                        }


                                        ++site.revealVisibleCells;


                                        // ---------------------------------
                                        // This Kinect pixel has crossed
                                        // the buried plane.
                                        //
                                        // Build one projected quad for it.
                                        // Six vertices = two triangles.
                                        // ---------------------------------

                                        const ofVec2f c0 =
                                            rotateLocal(
                                                localX,
                                                localY
                                            );

                                        const ofVec2f c1 =
                                            rotateLocal(
                                                localX + step,
                                                localY
                                            );

                                        const ofVec2f c2 =
                                            rotateLocal(
                                                localX + step,
                                                localY + step
                                            );

                                        const ofVec2f c3 =
                                            rotateLocal(
                                                localX,
                                                localY + step
                                            );


                                        const ofVec2f p0 =
                                            kinectProjector->
                                                kinectCoordToProjCoord(
                                                    site.position.x +
                                                        c0.x,
                                                    site.position.y +
                                                        c0.y
                                                );

                                        const ofVec2f p1 =
                                            kinectProjector->
                                                kinectCoordToProjCoord(
                                                    site.position.x +
                                                        c1.x,
                                                    site.position.y +
                                                        c1.y
                                                );

                                        const ofVec2f p2 =
                                            kinectProjector->
                                                kinectCoordToProjCoord(
                                                    site.position.x +
                                                        c2.x,
                                                    site.position.y +
                                                        c2.y
                                                );

                                        const ofVec2f p3 =
                                            kinectProjector->
                                                kinectCoordToProjCoord(
                                                    site.position.x +
                                                        c3.x,
                                                    site.position.y +
                                                        c3.y
                                                );


                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p0.x,
                                                p0.y,
                                                0.0f
                                            )
                                        );

                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p1.x,
                                                p1.y,
                                                0.0f
                                            )
                                        );

                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p2.x,
                                                p2.y,
                                                0.0f
                                            )
                                        );


                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p0.x,
                                                p0.y,
                                                0.0f
                                            )
                                        );

                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p2.x,
                                                p2.y,
                                                0.0f
                                            )
                                        );

                                        site.revealMaskMesh.addVertex(
                                            ofVec3f(
                                                p3.x,
                                                p3.y,
                                                0.0f
                                            )
                                        );
                                    }
                                }
                            }
                        }


                        // ------------------------------------------------
                        // TRUE ARTIFACT COMPLETION
                        //
                        // 99.5% is treated as visually complete so
                        // one noisy Kinect pixel cannot trap a child
                        // at 99.9% forever.
                        // ------------------------------------------------

                        if (site.revealEligibleCells > 0)
                        {
                            site.actualRevealFraction =
                                static_cast<float>(
                                    site.revealVisibleCells
                                ) /
                                static_cast<float>(
                                    site.revealEligibleCells
                                );


                            static float lastRevealDebug = -1000.0f;

                            if (
                                maskNow - lastRevealDebug >= 1.0f
                               )
                            {
                                lastRevealDebug = maskNow;

                                std::cout
                                    << "[DIG REVEAL] "
                                    << site.actualRevealFraction * 100.0f
                                    << "% visible"
                                    << std::endl;
                            }

                            if (
                                !site.completionLatched &&
                                site.actualRevealFraction >=
                                    0.995f
                               )
                            {
                                site.completionLatched =
                                    true;

                                site.completionTime =
                                    ofGetElapsedTimef();


                                std::cout
                                    << "[DIG COMPLETE] "
                                    << "reveal="
                                    << site.actualRevealFraction *
                                           100.0f
                                    << "% — admire for 10 sec"
                                    << std::endl;
                            }
                        }


                        site.revealMaskMesh.draw();
                    }


                    // -----------------------------------------
                    // Restore visible color rendering.
                    // Only stencil==1 may now draw.
                    // -----------------------------------------

                    glColorMask(
                        GL_TRUE,
                        GL_TRUE,
                        GL_TRUE,
                        GL_TRUE
                    );

                    glStencilFunc(
                        GL_EQUAL,
                        1,
                        0xFF
                    );

                    glStencilOp(
                        GL_KEEP,
                        GL_KEEP,
                        GL_KEEP
                    );


                    // =========================================
                    // DRAW ONE ROTATED ARTIFACT
                    //
                    // The PNG remains a single high-resolution
                    // image. Only the stencil decides which
                    // portions of it are currently uncovered.
                    // =========================================

                    const float objectScale =
                        site.buriedObjectType == 1
                            ? 0.40f
                            : 0.95f;


                    const float imageW =
                        buriedImage->getWidth();

                    const float imageH =
                        buriedImage->getHeight();


                    if (imageW > 0.0f &&
                        imageH > 0.0f &&
                        site.objectPlacementReady)
                    {
                        const float imageAspect =
                            imageW / imageH;


                        const float maxKinectSize =
                            digSiteRadius *
                            2.0f *
                            objectScale;


                        float objectKinectW;
                        float objectKinectH;


                        if (imageAspect >= 1.0f)
                        {
                            objectKinectW =
                                maxKinectSize;

                            objectKinectH =
                                objectKinectW /
                                imageAspect;
                        }
                        else
                        {
                            objectKinectH =
                                maxKinectSize;

                            objectKinectW =
                                objectKinectH *
                                imageAspect;
                        }


                        const float angle =
                            ofDegToRad(
                                site.objectRotationDeg
                            );


                        const float ca =
                            std::cos(angle);

                        const float sa =
                            std::sin(angle);


                        auto rotateCorner =
                            [ca, sa](
                                float x,
                                float y
                            )
                            {
                                return ofVec2f(
                                    ca * x - sa * y,
                                    sa * x + ca * y
                                );
                            };


                        const float halfW =
                            objectKinectW *
                            0.5f;

                        const float halfH =
                            objectKinectH *
                            0.5f;


                        const ofVec2f k0 =
                            rotateCorner(
                                -halfW,
                                -halfH
                            );

                        const ofVec2f k1 =
                            rotateCorner(
                                 halfW,
                                -halfH
                            );

                        const ofVec2f k2 =
                            rotateCorner(
                                 halfW,
                                 halfH
                            );

                        const ofVec2f k3 =
                            rotateCorner(
                                -halfW,
                                 halfH
                            );


                        const ofVec2f p0 =
                            kinectProjector->
                                kinectCoordToProjCoord(
                                    site.position.x + k0.x,
                                    site.position.y + k0.y
                                );

                        const ofVec2f p1 =
                            kinectProjector->
                                kinectCoordToProjCoord(
                                    site.position.x + k1.x,
                                    site.position.y + k1.y
                                );

                        const ofVec2f p2 =
                            kinectProjector->
                                kinectCoordToProjCoord(
                                    site.position.x + k2.x,
                                    site.position.y + k2.y
                                );

                        const ofVec2f p3 =
                            kinectProjector->
                                kinectCoordToProjCoord(
                                    site.position.x + k3.x,
                                    site.position.y + k3.y
                                );


                        ofMesh artifactMesh;

                        artifactMesh.setMode(
                            OF_PRIMITIVE_TRIANGLES
                        );


                        // Triangle 1
                        artifactMesh.addVertex(
                            ofVec3f(
                                p0.x,
                                p0.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                0.0f,
                                0.0f
                            )
                        );


                        artifactMesh.addVertex(
                            ofVec3f(
                                p1.x,
                                p1.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                imageW,
                                0.0f
                            )
                        );


                        artifactMesh.addVertex(
                            ofVec3f(
                                p2.x,
                                p2.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                imageW,
                                imageH
                            )
                        );


                        // Triangle 2
                        artifactMesh.addVertex(
                            ofVec3f(
                                p0.x,
                                p0.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                0.0f,
                                0.0f
                            )
                        );


                        artifactMesh.addVertex(
                            ofVec3f(
                                p2.x,
                                p2.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                imageW,
                                imageH
                            )
                        );


                        artifactMesh.addVertex(
                            ofVec3f(
                                p3.x,
                                p3.y,
                                0.0f
                            )
                        );

                        artifactMesh.addTexCoord(
                            ofVec2f(
                                0.0f,
                                imageH
                            )
                        );


                        ofSetColor(255);

                        buriedImage->
                            getTexture()
                            .bind();

                        artifactMesh.draw();

                        buriedImage->
                            getTexture()
                            .unbind();
                    }


                    glDisable(
                        GL_STENCIL_TEST
                    );
                }
                else
                {
                    static bool warnedNoStencil =
                        false;

                    if (!warnedNoStencil)
                    {
                        std::cout
                            << "[DIG] WARNING: no stencil buffer"
                            << std::endl;

                        warnedNoStencil =
                            true;
                    }
                }
            }


            ofSetColor(255);
        }


        // ====================================================
        // THICK EXCAVATION BORDER
        //
        // Before digging starts this is the normal amber/red
        // target circle. Once digging begins the expanding
        // textured rock rim above replaces it.
        // ====================================================

        if (!diggingStartedForRim)
        {
            ofNoFill();


            // Dark backing improves contrast against bright sand.
            ofSetLineWidth(12.0f);

        ofSetColor(
            35,
            15,
            15,
            190
        );


        ofDrawEllipse(
            center.x,
            center.y,
            radiusX * 2.0f,
            radiusY * 2.0f
        );


        // Main bright boundary.
        ofSetLineWidth(7.0f);


        // During the five-second arming period use amber.
        // After the reference is frozen, use red.
        if (!digReferenceFrozen)
        {
            ofSetColor(
                255,
                185,
                40,
                255
            );
        }
        else
        {
            ofSetColor(
                235,
                35,
                35,
                255
            );
        }


        ofDrawEllipse(
            center.x,
            center.y,
            radiusX * 2.0f,
            radiusY * 2.0f
        );

        } // end pre-dig amber/red target circle


        // ====================================================
        // RED X
        //
        // Keep the X only until real excavation begins.
        // Each depth is relative to that sample's frozen
        // starting elevation.
        // ====================================================

        // Once excavation begins, the rock-rim state is latched,
        // so the original X can never reappear later if sand is
        // pushed back over part of the site.
        bool showDigX =
            !diggingStartedForRim;


        if (showDigX)
        {
            const float xSize =
                std::min(
                    radiusX,
                    radiusY
                ) * 0.22f;


            ofSetLineWidth(9.0f);

            ofSetColor(
                35,
                15,
                15,
                200
            );


            ofDrawLine(
                center.x - xSize,
                center.y - xSize,
                center.x + xSize,
                center.y + xSize
            );

            ofDrawLine(
                center.x + xSize,
                center.y - xSize,
                center.x - xSize,
                center.y + xSize
            );


            ofSetLineWidth(5.0f);

            ofSetColor(
                240,
                35,
                35,
                255
            );


            ofDrawLine(
                center.x - xSize,
                center.y - xSize,
                center.x + xSize,
                center.y + xSize
            );

            ofDrawLine(
                center.x + xSize,
                center.y - xSize,
                center.x - xSize,
                center.y + xSize
            );
        }


        // ====================================================
        // Temporary diagnostics
        // ====================================================

        ofFill();


        if (!digReferenceFrozen)
        {
            ofSetColor(
                255,
                255,
                255,
                255
            );


            const std::string text =
                "READY " +
                ofToString(
                    static_cast<int>(
                        ceil(remaining)
                    )
                );


            ofDrawBitmapStringHighlight(
                text,
                center.x - 28.0f,
                center.y +
                    radiusY +
                    18.0f
            );
        }
        else
        {
            const int percent =
                static_cast<int>(
                    site.excavationProgress *
                    100.0f
                );


            ofSetColor(
                255,
                255,
                255,
                255
            );


            ofDrawBitmapStringHighlight(
                ofToString(percent) + "%",
                center.x - 14.0f,
                center.y +
                    radiusY +
                    18.0f
            );
        }
    }


    ofPopStyle();
}


// ============================================================
// FULL-ROI FOSSIL FINDER
//
// IMPORTANT:
//
// This intentionally uses the SAME archaeological reveal
// method as the working DIG mode.
//
// The only differences are:
//
//   * one large T-Rex
//   * nearly the full ROI
//   * no target graphics
//   * no rock rim
//   * no completion animation
//   * no respawn
//
// Reveal rule:
//
//        live sand elevation
//                 <=
//        fixed fossil plane
//
// Exactly the same basic rule used by DIG.
// ============================================================


void ofApp::startFossilFinder()
{
    if(!MagicSandModeButtons::shown(0))return; // MODE_BUTTONS_V37

    if (!kinectProjector)
        return;

    // TopoPlay modification — 2026-10-24: select one fossil for this dig round.
    if(!fossilCatalog.choose()){
        fossilFinder.enabled=false;
        std::cout<<"[FOSSIL] ERROR: no readable fossil image available"<<std::endl;
        return;
    }
    fossilFinder.objectScale=ofRandom(0.65f,1.0f);
    std::cout<<"[FOSSIL ASSET] "<<fossilCatalog.selectedPath()
        <<" size="<<fossilFinder.objectScale<<std::endl;
    fossilFinder.enabled = true;
    fossilGeometry.reset(); // FOSSIL_ROI_V33

    fossilFinder.activatedTime =
        ofGetElapsedTimef();

    fossilFinder.referenceFrozen = false;

    fossilFinder.referenceROI =
        kinectProjector->getKinectROI();

    fossilFinder.objectPlacementReady = false;

    // Same idea as DIG: every new buried object can have its own
    // orientation.
    fossilFinder.objectRotationDeg =
        ofRandom(
            0.0f,
            360.0f
        );

    fossilFinder.revealMaskMesh.clear();

    fossilFinder.lastRevealMaskUpdate =
        -1000.0f;

    fossilFinder.revealEligibleCells = 0;
    fossilFinder.revealVisibleCells = 0;
    fossilFinder.actualRevealFraction = 0.0f;

    std::cout
        << "[FOSSIL] ON - waiting 5 seconds before terrain reference"
        << std::endl;
}


void ofApp::stopFossilFinder()
{
    fossilFinder.enabled = false;

    fossilFinder.referenceFrozen = false;
    fossilFinder.objectPlacementReady = false;

    fossilFinder.revealMaskMesh.clear();

    fossilFinder.lastRevealMaskUpdate =
        -1000.0f;

    fossilFinder.revealEligibleCells = 0;
    fossilFinder.revealVisibleCells = 0;
    fossilFinder.actualRevealFraction = 0.0f;

    std::cout
        << "[FOSSIL] OFF"
        << std::endl;
}


// ------------------------------------------------------------
// Freeze Fossil Finder reference.
//
// This follows DIG's buried-object placement method:
//
//     1. Look at the starting surface.
//     2. Find the LOWEST starting terrain under the fossil.
//     3. Put one archaeological plane 20 mm below it.
//
// Therefore:
//
//     shallow/low sand -> reaches fossil sooner
//
//     tall mound      -> requires more digging
//
// Exactly the same geometry principle as DIG.
// ------------------------------------------------------------

void ofApp::freezeFossilFinderReference()
{
    if (!fossilFinder.enabled ||
        !kinectProjector)
    {
        return;
    }

    fossilFinder.referenceROI =
        kinectProjector->getKinectROI();

    const ofRectangle& roi =
        fossilFinder.referenceROI;

    if (roi.width < 100.0f ||
        roi.height < 100.0f)
    {
        return;
    }

    ofImage& fossilImage=fossilCatalog.selectedImage();
    if(!fossilImage.isAllocated())return;

    const float imageW =
        fossilImage.getWidth();

    const float imageH =
        fossilImage.getHeight();

    // Fit the final rotated footprint, shared with reference sampling/drawing.
    const auto objectSize=FossilBoxGeometry::fit(roi,imageW/imageH,fossilFinder.objectRotationDeg);
    const float objectKinectW=objectSize.x*fossilFinder.objectScale,objectKinectH=objectSize.y*fossilFinder.objectScale;

    const ofVec2f objectCenter(
        roi.x +
            roi.width * 0.5f,

        roi.y +
            roi.height * 0.5f
    );

    const float halfW =
        objectKinectW *
        0.5f;

    const float halfH =
        objectKinectH *
        0.5f;

    const float angle =
        ofDegToRad(
            fossilFinder.objectRotationDeg
        );

    const float ca =
        std::cos(angle);

    const float sa =
        std::sin(angle);

    auto rotateLocal =
        [ca, sa](
            float x,
            float y
        )
        {
            return ofVec2f(
                ca * x -
                    sa * y,

                sa * x +
                    ca * y
            );
        };

    float lowestReference =
        1.0e9f;

    int referenceSamples = 0;

    // EXACTLY like DIG's dense fossil mask:
    // use roughly one Kinect pixel.
    const float step =
        1.0f;

    for (
        float localY = -halfH;
        localY < halfH;
        localY += step
        )
    {
        for (
            float localX = -halfW;
            localX < halfW;
            localX += step
            )
        {
            const float centerX =
                localX +
                step * 0.5f;

            const float centerY =
                localY +
                step * 0.5f;

            // Only consider actual opaque fossil pixels.
            const float imageU =
                ofClamp(
                    (
                        centerX +
                        halfW
                    ) /
                        objectKinectW,
                    0.0f,
                    0.999999f
                );

            const float imageV =
                ofClamp(
                    (
                        centerY +
                        halfH
                    ) /
                        objectKinectH,
                    0.0f,
                    0.999999f
                );

            const int imageX =
                ofClamp(
                    static_cast<int>(
                        imageU *
                        imageW
                    ),
                    0,
                    static_cast<int>(
                        imageW
                    ) - 1
                );

            const int imageY =
                ofClamp(
                    static_cast<int>(
                        imageV *
                        imageH
                    ),
                    0,
                    static_cast<int>(
                        imageH
                    ) - 1
                );

            const ofColor sourcePixel =
                fossilImage.getColor(
                    imageX,
                    imageY
                );

            if (sourcePixel.a <= 24)
            {
                continue;
            }

            const ofVec2f rotatedCenter =
                rotateLocal(
                    centerX,
                    centerY
                );

            const ofVec2f sampleKinect =
                objectCenter +
                rotatedCenter;

            if (!roi.inside(
                    sampleKinect
                ))
            {
                continue;
            }

            const float elevation =
                kinectProjector->
                    elevationAtKinectCoord(
                        sampleKinect.x,
                        sampleKinect.y
                    );

            lowestReference =
                std::min(
                    lowestReference,
                    elevation
                );

            ++referenceSamples;
        }
    }

    if (referenceSamples <= 0 ||
        lowestReference > 1.0e8f)
    {
        std::cout
            << "[FOSSIL] ERROR: could not establish reference plane"
            << std::endl;

        return;
    }

    // EXACT DIG placement depth.
    fossilFinder.objectPlaneElevation =
        lowestReference -
        20.0f;

    fossilFinder.objectPlacementReady =
        true;

    fossilFinder.referenceFrozen =
        true;

    fossilFinder.revealMaskMesh.clear();

    fossilFinder.lastRevealMaskUpdate =
        -1000.0f;

    fossilFinder.revealEligibleCells = 0;
    fossilFinder.revealVisibleCells = 0;
    fossilFinder.actualRevealFraction = 0.0f;

    std::cout
        << "[FOSSIL] DIG-style reference frozen"
        << " plane="
        << fossilFinder.objectPlaneElevation
        << " rotation="
        << fossilFinder.objectRotationDeg
        << " samples="
        << referenceSamples
        << std::endl;
}


void ofApp::updateFossilFinder()
{
    if (!fossilFinder.enabled ||
        !kinectProjector)
    {
        return;
    }

    if (kinectProjector->
            GetApplicationState() !=
        KinectProjector::
            APPLICATION_STATE_RUNNING)
    {
        return;
    }

    // A fresh calibration/ROI invalidates the buried reference and its mesh.
    const auto liveROI=kinectProjector->getKinectROI();
    const bool changedGeometry=fossilGeometry.changed(*kinectProjector);
    if(changedGeometry||liveROI!=fossilFinder.referenceROI){
        startFossilFinder(); // retain mode ON; repeat the existing settling delay
        std::cout<<"[FOSSIL ROI V33] Refreshed ROI/calibration: "
            <<liveROI.x<<","<<liveROI.y<<","<<liveROI.width<<","<<liveROI.height<<std::endl;
    }

    // Same five-second settling delay as DIG.
    if (!fossilFinder.referenceFrozen)
    {
        const float elapsed =
            ofGetElapsedTimef() -
            fossilFinder.activatedTime;

        if (elapsed >= 5.0f)
        {
            freezeFossilFinderReference();
        }
    }
}


// ------------------------------------------------------------
// Draw the round-selected DIG-style fossil or fossil group.
//
// The reveal code below intentionally mirrors DIG's dense,
// one-Kinect-pixel CT-scan reveal.
// ------------------------------------------------------------

void ofApp::drawFossilFinder()
{
    if (!fossilFinder.enabled ||
        !fossilFinder.referenceFrozen ||
        !fossilFinder.objectPlacementReady ||
        !kinectProjector)
    {
        return;
    }

    ofImage& fossilImage=fossilCatalog.selectedImage();
    if(!fossilImage.isAllocated())return;

    const ofRectangle& roi =
        fossilFinder.referenceROI;

    const float imageW =
        fossilImage.getWidth();

    const float imageH =
        fossilImage.getHeight();

    if (imageW <= 0.0f ||
        imageH <= 0.0f)
    {
        return;
    }

    // Fit the final rotated footprint, shared with reference sampling/drawing.
    const auto objectSize=FossilBoxGeometry::fit(roi,imageW/imageH,fossilFinder.objectRotationDeg);
    const float objectKinectW=objectSize.x*fossilFinder.objectScale,objectKinectH=objectSize.y*fossilFinder.objectScale;

    const ofVec2f objectCenter(
        roi.x +
            roi.width * 0.5f,

        roi.y +
            roi.height * 0.5f
    );

    const float halfW =
        objectKinectW *
        0.5f;

    const float halfH =
        objectKinectH *
        0.5f;

    const float angle =
        ofDegToRad(
            fossilFinder.objectRotationDeg
        );

    const float ca =
        std::cos(angle);

    const float sa =
        std::sin(angle);

    auto rotateLocal =
        [ca, sa](
            float x,
            float y
        )
        {
            return ofVec2f(
                ca * x -
                    sa * y,

                sa * x +
                    ca * y
            );
        };

    // ========================================================
    // SAME REVEAL UPDATE RATE AS DIG
    // ========================================================

    const float maskNow =
        ofGetElapsedTimef();

    if (
        maskNow -
            fossilFinder.lastRevealMaskUpdate >=
            0.20f
        ||
        fossilFinder
            .revealMaskMesh
            .getNumVertices() == 0
       )
    {
        fossilFinder.lastRevealMaskUpdate =
            maskNow;

        fossilFinder.revealMaskMesh.clear();

        fossilFinder.revealMaskMesh.setMode(
            OF_PRIMITIVE_TRIANGLES
        );

        fossilFinder.revealEligibleCells =
            0;

        fossilFinder.revealVisibleCells =
            0;

        fossilFinder.actualRevealFraction =
            0.0f;

        // EXACTLY the same sensor-scale resolution used by
        // DIG's dense CT-scan reveal.
        const float step =
            1.0f;

        for (
            float localY = -halfH;
            localY < halfH;
            localY += step
            )
        {
            for (
                float localX = -halfW;
                localX < halfW;
                localX += step
                )
            {
                const float centerX =
                    localX +
                    step * 0.5f;

                const float centerY =
                    localY +
                    step * 0.5f;

                // --------------------------------------------
                // COUNT ONLY ACTUAL PNG CONTENT
                //
                // SAME as DIG.
                // --------------------------------------------

                const float imageU =
                    ofClamp(
                        (
                            centerX +
                            halfW
                        ) /
                            objectKinectW,
                        0.0f,
                        0.999999f
                    );

                const float imageV =
                    ofClamp(
                        (
                            centerY +
                            halfH
                        ) /
                            objectKinectH,
                        0.0f,
                        0.999999f
                    );

                const int imageX =
                    ofClamp(
                        static_cast<int>(
                            imageU *
                            imageW
                        ),
                        0,
                        static_cast<int>(
                            imageW
                        ) - 1
                    );

                const int imageY =
                    ofClamp(
                        static_cast<int>(
                            imageV *
                            imageH
                        ),
                        0,
                        static_cast<int>(
                            imageH
                        ) - 1
                    );

                const ofColor sourcePixel =
                    fossilImage.getColor(
                        imageX,
                        imageY
                    );

                if (sourcePixel.a <= 24)
                {
                    continue;
                }

                ++fossilFinder
                    .revealEligibleCells;

                const ofVec2f rotatedCenter =
                    rotateLocal(
                        centerX,
                        centerY
                    );

                const ofVec2f sampleKinect =
                    objectCenter +
                    rotatedCenter;

                if (!roi.inside(
                        sampleKinect
                    ))
                {
                    continue;
                }

                // ============================================
                // EXACT DIG REVEAL TEST
                //
                // Sand is still above archaeological plane:
                //      hidden
                //
                // Sand reaches/passes archaeological plane:
                //      visible
                // ============================================

                const float liveElevation =
                    kinectProjector->
                        elevationAtKinectCoord(
                            sampleKinect.x,
                            sampleKinect.y
                        );

                if (
                    liveElevation >
                    fossilFinder
                        .objectPlaneElevation
                   )
                {
                    continue;
                }

                ++fossilFinder
                    .revealVisibleCells;

                // Same rotated one-pixel projected quad used
                // by DIG.

                const ofVec2f c0 =
                    rotateLocal(
                        localX,
                        localY
                    );

                const ofVec2f c1 =
                    rotateLocal(
                        localX + step,
                        localY
                    );

                const ofVec2f c2 =
                    rotateLocal(
                        localX + step,
                        localY + step
                    );

                const ofVec2f c3 =
                    rotateLocal(
                        localX,
                        localY + step
                    );

                const ofVec2f p0 =
                    kinectProjector->
                        kinectCoordToProjCoord(
                            objectCenter.x +
                                c0.x,

                            objectCenter.y +
                                c0.y
                        );

                const ofVec2f p1 =
                    kinectProjector->
                        kinectCoordToProjCoord(
                            objectCenter.x +
                                c1.x,

                            objectCenter.y +
                                c1.y
                        );

                const ofVec2f p2 =
                    kinectProjector->
                        kinectCoordToProjCoord(
                            objectCenter.x +
                                c2.x,

                            objectCenter.y +
                                c2.y
                        );

                const ofVec2f p3 =
                    kinectProjector->
                        kinectCoordToProjCoord(
                            objectCenter.x +
                                c3.x,

                            objectCenter.y +
                                c3.y
                        );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p0.x,
                            p0.y,
                            0.0f
                        )
                    );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p1.x,
                            p1.y,
                            0.0f
                        )
                    );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p2.x,
                            p2.y,
                            0.0f
                        )
                    );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p0.x,
                            p0.y,
                            0.0f
                        )
                    );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p2.x,
                            p2.y,
                            0.0f
                        )
                    );

                fossilFinder
                    .revealMaskMesh
                    .addVertex(
                        ofVec3f(
                            p3.x,
                            p3.y,
                            0.0f
                        )
                    );
            }
        }

        if (
            fossilFinder
                .revealEligibleCells >
            0
           )
        {
            fossilFinder
                .actualRevealFraction =
                static_cast<float>(
                    fossilFinder
                        .revealVisibleCells
                ) /
                static_cast<float>(
                    fossilFinder
                        .revealEligibleCells
                );
        }

        static float lastDebug =
            -1000.0f;

        if (
            maskNow -
            lastDebug >=
            1.0f
           )
        {
            lastDebug =
                maskNow;

            std::cout
                << "[FOSSIL DIG-REVEAL] "
                << fossilFinder
                       .actualRevealFraction *
                       100.0f
                << "% visible"
                << std::endl;
        }
    }

    if (
        fossilFinder
            .revealMaskMesh
            .getNumVertices() ==
        0
       )
    {
        return;
    }

    // ========================================================
    // SAME STENCIL METHOD AS DIG
    // ========================================================

    GLint stencilBits =
        0;

    glGetIntegerv(
        GL_STENCIL_BITS,
        &stencilBits
    );

    if (stencilBits <= 0)
    {
        return;
    }

    glEnable(
        GL_STENCIL_TEST
    );

    glClearStencil(0);

    glClear(
        GL_STENCIL_BUFFER_BIT
    );

    glColorMask(
        GL_FALSE,
        GL_FALSE,
        GL_FALSE,
        GL_FALSE
    );

    glStencilFunc(
        GL_ALWAYS,
        1,
        0xFF
    );

    glStencilOp(
        GL_KEEP,
        GL_KEEP,
        GL_REPLACE
    );

    fossilFinder
        .revealMaskMesh
        .draw();

    glColorMask(
        GL_TRUE,
        GL_TRUE,
        GL_TRUE,
        GL_TRUE
    );

    glStencilFunc(
        GL_EQUAL,
        1,
        0xFF
    );

    glStencilOp(
        GL_KEEP,
        GL_KEEP,
        GL_KEEP
    );

    // ========================================================
    // DRAW ONE ROTATED LARGE FOSSIL
    //
    // Same single-image approach as DIG.
    // ========================================================

    const ofVec2f k0 =
        rotateLocal(
            -halfW,
            -halfH
        );

    const ofVec2f k1 =
        rotateLocal(
             halfW,
            -halfH
        );

    const ofVec2f k2 =
        rotateLocal(
             halfW,
             halfH
        );

    const ofVec2f k3 =
        rotateLocal(
            -halfW,
             halfH
        );

    const ofVec2f p0 =
        kinectProjector->
            kinectCoordToProjCoord(
                objectCenter.x +
                    k0.x,

                objectCenter.y +
                    k0.y
            );

    const ofVec2f p1 =
        kinectProjector->
            kinectCoordToProjCoord(
                objectCenter.x +
                    k1.x,

                objectCenter.y +
                    k1.y
            );

    const ofVec2f p2 =
        kinectProjector->
            kinectCoordToProjCoord(
                objectCenter.x +
                    k2.x,

                objectCenter.y +
                    k2.y
            );

    const ofVec2f p3 =
        kinectProjector->
            kinectCoordToProjCoord(
                objectCenter.x +
                    k3.x,

                objectCenter.y +
                    k3.y
            );

    ofMesh fossilMesh;

    fossilMesh.setMode(
        OF_PRIMITIVE_TRIANGLES
    );

    // Triangle 1
    fossilMesh.addVertex(
        ofVec3f(
            p0.x,
            p0.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            0.0f,
            0.0f
        )
    );

    fossilMesh.addVertex(
        ofVec3f(
            p1.x,
            p1.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            imageW,
            0.0f
        )
    );

    fossilMesh.addVertex(
        ofVec3f(
            p2.x,
            p2.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            imageW,
            imageH
        )
    );

    // Triangle 2
    fossilMesh.addVertex(
        ofVec3f(
            p0.x,
            p0.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            0.0f,
            0.0f
        )
    );

    fossilMesh.addVertex(
        ofVec3f(
            p2.x,
            p2.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            imageW,
            imageH
        )
    );

    fossilMesh.addVertex(
        ofVec3f(
            p3.x,
            p3.y,
            0.0f
        )
    );

    fossilMesh.addTexCoord(
        ofVec2f(
            0.0f,
            imageH
        )
    );

    ofSetColor(255);

    fossilImage
        .getTexture()
        .bind();

    fossilMesh.draw();

    fossilImage
        .getTexture()
        .unbind();

    glDisable(
        GL_STENCIL_TEST
    );

    ofSetColor(255);
}



void ofApp::updateProjectorComposite()
{
    if(shuttingDown)return;
    if (kinectProjector->GetApplicationState() !=
        KinectProjector::APPLICATION_STATE_RUNNING)
    {
        // Drain the previous RUN handoff once; no sync work during calibration.
        projectorFrameSync.suspend();
        return;
    }

    projectorFrameSync.beforeWrite();
    projectorComposite.begin();

    // Build exactly the same final image that used to be assembled
    // separately in the projector-window draw callback.
    ofClear(0, 0, 0, 255);

    if (fossilFinder.enabled)
    {
        // ====================================================
        // LIGHTWEIGHT FOSSIL TERRAIN
        //
        // Fossil Mode deliberately does NOT render the normal
        // multi-layer elevation terrain.
        //
        // Every terrain level is visually just sand, so draw one
        // simple sand image across the projector and then reveal
        // the fossil through its excavation mask.
        // ====================================================

        static ofImage fossilSandImage;
        static bool fossilSandImageTried = false;
        static bool fossilSandImageLoaded = false;

        if (!fossilSandImageTried)
        {
            fossilSandImageTried = true;

            fossilSandImageLoaded =
                fossilSandImage.load(
                    "textures/sandygrass.jpg"
                );

            std::cout
                << "[FOSSIL SAND] "
                << (fossilSandImageLoaded ? "OK" : "FAILED")
                << std::endl;
        }

        // FOSSIL_SURFACE_V35: use current sensor ROI and calibrated projection.
        fossilRoiSurface.draw(kinectProjector,
            fossilSandImageLoaded&&fossilSandImage.isAllocated()?&fossilSandImage:nullptr);

        // Reveal the one large buried fossil.
        drawFossilFinder();

        // Keep the physical FOSSIL/DIG controls live and visible.
        drawRingButtonTest();
    }
    else
    {
        // ====================================================
        // NORMAL MAGIC-SAND / DIG MODE
        // ====================================================

        sandSurfaceRenderer->drawProjectorWindow();
        rainRunoff.draw();idleDemo.drawWater(); // IDLE_DEMO_V1
        if(!idleDemo.running())volcano.draw();idleDemo.drawVolcano(); // IDLE_DEMO_V1
        mapGameController.drawProjectorWindow();
        boidGameController.drawProjectorWindow();
        kinectProjector->drawProjectorWindow();

        drawRingButtonTest();

        if (digSiteModeEnabled)
        {
            drawDigSites();
        }
    }

    if(!idleDemo.running())rainRunoff.drawFlash();idleDemo.drawTop(); // IDLE_DEMO_V1
    ceilingMask.draw(); // CONTROLS_V3: final black mask above the ceiling
    projectorComposite.end();
    projectorFrameSync.afterWrite();
}


void ofApp::draw()
{
    if(shuttingDown){
        ofBackground(24);ofSetColor(255);
        ofDrawBitmapString("Closing TopoPlay... waiting for Kinect cleanup",24,40);
        return;
    }
    layoutOperatorGui(); // EDGE_GUI_V6: includes expanded folders/resizes
    diagMainDrawCount++;
    float x = mainWindowROI.x;
    float y = mainWindowROI.y;
    float w = mainWindowROI.width;
    float h = mainWindowROI.height;

    if (kinectProjector->GetApplicationState() ==
        KinectProjector::APPLICATION_STATE_RUNNING)
    {
        // Show the EXACT finished projector image in the RUN window.
        if (kinectProjector->isCameraPreviewEnabled())
            kinectProjector->drawMainWindow(x, y, w, h);
        else
            projectorComposite.draw(x, y, w, h);

        // Draw controls over the selected main-window view.
        sandSurfaceRenderer->drawGuiOnly();
        boidGameController.drawGuiOnly();
        if (!kinectProjector->isCameraPreviewEnabled())
            kinectProjector->drawGuiOnly();
    }
    else
    {
        // Setup/calibration operation remains unchanged.
        kinectProjector->drawMainWindow(x, y, w, h);
    }
    if(projectorColorGui && projectorColorVisible) {
        projectorColorGui->draw();
    }


    cpuLimiter.draw(); // MAGICSAND_CPU_GUI_V1

    triggerControls.draw(); // TRIGGER_CONTROLS_V1

    idleDemo.drawGui(); // IDLE_DEMO_V1
    MagicSandPanelScroll::draw(ofGetWidth(),operatorLeftScroll,operatorLeftMax); // EDITOR_SCROLL_V58
}

void ofApp::drawProjWindow(ofEventArgs &args)
{
    if(shuttingDown){ofClear(0,0,0,255);return;}
    diagProjDrawCount++;
    if (kinectProjector->GetApplicationState() ==
        KinectProjector::APPLICATION_STATE_RUNNING)
    {
        // Apply correction only to the projector; PC preview stays original.
        projectorFrameSync.beforeRead();
        drawCorrectedProjector();
        projectorFrameSync.afterRead();
    }
    else
    {
        // Calibration/setup still uses KinectProjector directly.
        kinectProjector->drawProjectorWindow();
    }
}

void ofApp::keyPressed(int key) 
{
    if(shuttingDown)return;
    idleDemo.noteActivity();if(idleDemo.keyPressed(key))return; // IDLE_DEMO_V1

    if(triggerControls.keyPressed(key))return; // TRIGGER_CONTROLS_V1

    if(cpuLimiter.keyPressed(key))return; // MAGICSAND_CPU_GUI_V1

    if(key=='p' || key=='P') {
        projectorColorVisible=!projectorColorVisible;
        if(!projectorColorVisible && projectorColorDirty) saveProjectorColor();
        return;
    }
	if (key == 'c')
	{
		kinectProjector->SaveKinectColorImage();
	}
	else if (key == 'd')
	{
		kinectProjector->SaveFilteredDepthImage();
	}
	else if (key == ' ')
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && 
			boidGameController.isIdle()) // do not start map game if boidgame is not idle
		{
			if (mapGameController.isIdle())
			{
				mapGameController.setDebug(kinectProjector->getDumpDebugFiles());
				mapGameController.StartGame();
			}
			else
			{
				mapGameController.ButtonPressed();
			}
		}
		else if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_SETUP)
		{
			// Try to start the application
			kinectProjector->startApplication();
		}
	}
	else if (key == 'f' || key == 'r')
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING)
		{
			if (mapGameController.isIdle())
			{
				boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
				boidGameController.StartGame(2);
			}
			else 
			{
				mapGameController.EndButtonPressed();
			}
		}
	}
	else if (key == '1') // Absolute beginner
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && mapGameController.isIdle())
		{
			boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
			boidGameController.StartGame(0);
		}
	}
	else if (key == '2') 
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && mapGameController.isIdle())
		{
			boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
			boidGameController.StartGame(1);
		}
	}
	else if (key == '3')
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && mapGameController.isIdle())
		{
			boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
			boidGameController.StartGame(2);
		}
	}
	else if (key == '4')
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && mapGameController.isIdle())
		{
			boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
			boidGameController.StartGame(3);
		}
	}
	else if(key=='5'&&!MagicSandModeButtons::shown(0)){return;} // MODE_BUTTONS_V37
	else if (key == '5') // Fossil Finder emergency/manual toggle
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING)
		{
			if (fossilFinder.enabled)
			{
				stopFossilFinder();
			}
			else
			{
				if (digSiteModeEnabled)
				{
					digSiteModeEnabled = false;
					digSites.clear();
					digReferenceFrozen = false;
				}

				boidGameController.setRequestedFishCount(0);
				boidGameController.setRequestedRabbitCount(0);
				boidGameController.setRequestedSharkCount(0);
				startFossilFinder();
			}

			std::cout
				<< "[KEY CONTROL] FOSSIL "
				<< (fossilFinder.enabled ? "ON" : "OFF")
				<< std::endl;
		}
	}
	else if (key == 'm')
	{
		if (kinectProjector->GetApplicationState() == KinectProjector::APPLICATION_STATE_RUNNING && mapGameController.isIdle())
		{
			boidGameController.setDebug(kinectProjector->getDumpDebugFiles());
			boidGameController.StartSeekMotherGame();
		}
	}
	else if (key == 't')
	{
		mapGameController.setDebug(kinectProjector->getDumpDebugFiles());
		mapGameController.RealTimeTestMe();
	}
	else if (key == 'w')
	{
		mapGameController.setDebug(kinectProjector->getDumpDebugFiles());
		mapGameController.DebugTestMe();
	}
}

void ofApp::keyReleased(int key) {

}

void ofApp::mouseMoved(int x, int y) {

}

void ofApp::mouseDragged(int x, int y, int button) {
    if(shuttingDown)return;

	// We assume that we only use this during ROI annotation
	kinectProjector->mouseDragged((x-mainWindowROI.x)*kinectProjector->getKinectRes().x/std::max(1.0f,mainWindowROI.width), (y-mainWindowROI.y)*kinectProjector->getKinectRes().y/std::max(1.0f,mainWindowROI.height), button);
}

void ofApp::mousePressed(int x, int y, int button) 
{
    if(shuttingDown)return;
    idleDemo.noteActivity(); // IDLE_DEMO_V1
    if(MagicSandPanelScroll::handle(x,y,button,ofGetWidth(),ofGetHeight(),operatorLeftScroll,operatorLeftMax)){
        layoutOperatorGui();return;
    }

	if (mainWindowROI.inside((float)x, (float)y))
	{
		kinectProjector->mousePressed((x-mainWindowROI.x)*kinectProjector->getKinectRes().x/std::max(1.0f,mainWindowROI.width), (y-mainWindowROI.y)*kinectProjector->getKinectRes().y/std::max(1.0f,mainWindowROI.height), button);
	}
}

void ofApp::mouseReleased(int x, int y, int button) {
    if(shuttingDown)return;
	// We assume that we only use this during ROI annotation
	kinectProjector->mouseReleased((x-mainWindowROI.x)*kinectProjector->getKinectRes().x/std::max(1.0f,mainWindowROI.width), (y-mainWindowROI.y)*kinectProjector->getKinectRes().y/std::max(1.0f,mainWindowROI.height), button);

}

void ofApp::mouseEntered(int x, int y) {

}

void ofApp::mouseExited(int x, int y) {

}

void ofApp::windowResized(int w, int h) {

}

void ofApp::gotMessage(ofMessage msg) {

}

void ofApp::dragEvent(ofDragInfo dragInfo) {

}


// PROJECTOR_COLOR_V1: neutral-preserving output correction, P toggles controls.
void ofApp::setupProjectorColor()
{
    const std::string path=ofToDataPath("projector-color.json",true);
    try {
        if(ofFile::doesFileExist(path)) {
            auto j=ofLoadJson(path);
            auto value=[&](const char* key,float fallback,float lo,float hi) {
                float v=j.value(key,fallback);
                return std::isfinite(v) ? ofClamp(v,lo,hi) : fallback;
            };
            projectorRed=value("red",1,.5f,1.5f);
            projectorGreen=value("green",1,.5f,1.5f);
            projectorBlue=value("blue",1,.5f,1.5f);
            projectorSaturation=value("saturation",1,0,2);
            projectorColorEnabled=j.value("enabled",true);
        }
    } catch(const std::exception& e) {
        ofLogWarning("ProjectorColor")<<"Cannot load settings: "<<e.what();
    }
    projectorColorGui.reset(new ofxDatGui());
    projectorColorGui->setAutoDraw(false);
    projectorColorGui->addHeader("PROJECTOR COLOR (P SHOW/HIDE)",false);
    projectorColorGui->addToggle("Correction enabled",projectorColorEnabled);
    projectorColorGui->addSlider("Red strength",.5f,1.5f,projectorRed)->setPrecision(2);
    projectorColorGui->addSlider("Green strength",.5f,1.5f,projectorGreen)->setPrecision(2);
    projectorColorGui->addSlider("Blue strength",.5f,1.5f,projectorBlue)->setPrecision(2);
    projectorColorGui->addSlider("Color saturation",0,2,projectorSaturation)->setPrecision(2);
    projectorColorGui->addButton("Save projector colors");
    projectorColorGui->addButton("Reset projector colors");
    projectorColorGui->onSliderEvent(this,&ofApp::onProjectorColorSlider);
    projectorColorGui->onButtonEvent(this,&ofApp::onProjectorColorButton);
    projectorColorGui->onToggleEvent(this,&ofApp::onProjectorColorToggle);

    bool modern=ofIsGLProgrammableRenderer();
    bool rectangle=projectorComposite.getTexture().getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
    std::string vertex=modern ? R"GLSL(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 colorTexCoord;
void main(){colorTexCoord=texcoord;gl_Position=modelViewProjectionMatrix*position;}
)GLSL" : R"GLSL(#version 120
varying vec2 colorTexCoord;
void main(){colorTexCoord=gl_MultiTexCoord0.xy;gl_Position=ftransform();}
)GLSL";
    std::string fragment=modern ? "#version 150\nin vec2 colorTexCoord;\nout vec4 outputColor;\n" :
        "#version 120\nvarying vec2 colorTexCoord;\n";
    fragment+=rectangle ? "uniform sampler2DRect tex0;\n" : "uniform sampler2D tex0;\n";
    fragment+="uniform vec3 channelStrength;\nuniform float saturation;\nvoid main(){\nvec4 pixel=";
    fragment+=modern ? "texture(tex0,colorTexCoord);\n" :
        (rectangle ? "texture2DRect(tex0,colorTexCoord);\n" : "texture2D(tex0,colorTexCoord);\n");
    fragment+=R"GLSL(
vec3 c=pixel.rgb;
float chroma=max(max(c.r,c.g),c.b)-min(min(c.r,c.g),c.b);
float colored=smoothstep(0.02,0.15,chroma);
vec3 balanced=mix(c,c*channelStrength,colored);
float gray=dot(balanced,vec3(0.2126,0.7152,0.0722));
vec3 result=clamp(mix(vec3(gray),balanced,saturation),0.0,1.0);
)GLSL";
    fragment+=modern ? "outputColor=vec4(result,pixel.a);\n}\n" : "gl_FragColor=vec4(result,pixel.a);\n}\n";
    bool v=projectorColorShader.setupShaderFromSource(GL_VERTEX_SHADER,vertex);
    bool f=projectorColorShader.setupShaderFromSource(GL_FRAGMENT_SHADER,fragment);
    if(modern) projectorColorShader.bindDefaults();
    projectorColorReady=v && f && projectorColorShader.linkProgram();
    if(!projectorColorReady)
        ofLogError("ProjectorColor")<<"Shader unavailable; original projector image retained.";
}

void ofApp::drawCorrectedProjector()
{
    ofPushStyle();ofSetColor(255);
    bool apply=projectorColorReady && projectorColorEnabled &&
        (projectorRed!=1 || projectorGreen!=1 || projectorBlue!=1 || projectorSaturation!=1);
    if(apply) {
        projectorColorShader.begin();
        projectorColorShader.setUniform3f("channelStrength",projectorRed,projectorGreen,projectorBlue);
        projectorColorShader.setUniform1f("saturation",projectorSaturation);
    }
    projectorComposite.draw(0,0);
    if(apply) projectorColorShader.end();
    ofPopStyle();
}

void ofApp::saveProjectorColor()
{
    ofJson j;
    j["red"]=projectorRed;j["green"]=projectorGreen;j["blue"]=projectorBlue;
    j["saturation"]=projectorSaturation;j["enabled"]=projectorColorEnabled;
    try {
        if(ofSavePrettyJson(ofToDataPath("projector-color.json",true),j))
            projectorColorDirty=false;
        else {
            projectorColorChangedAt=ofGetElapsedTimef()+5;
            ofLogWarning("ProjectorColor")<<"Unable to save projector colors.";
        }
    } catch(const std::exception& e) {
        projectorColorChangedAt=ofGetElapsedTimef()+5;
        ofLogWarning("ProjectorColor")<<"Save failed: "<<e.what();
    }
}

void ofApp::onProjectorColorSlider(ofxDatGuiSliderEvent e)
{
    if(e.target->is("Red strength")) projectorRed=e.value;
    else if(e.target->is("Green strength")) projectorGreen=e.value;
    else if(e.target->is("Blue strength")) projectorBlue=e.value;
    else if(e.target->is("Color saturation")) projectorSaturation=e.value;
    projectorColorDirty=true;projectorColorChangedAt=ofGetElapsedTimef();
}
void ofApp::onProjectorColorToggle(ofxDatGuiToggleEvent e)
{
    projectorColorEnabled=e.checked;
    projectorColorDirty=true;projectorColorChangedAt=ofGetElapsedTimef();
}
void ofApp::onProjectorColorButton(ofxDatGuiButtonEvent e)
{
    if(e.target->is("Reset projector colors")) {
        projectorRed=projectorGreen=projectorBlue=projectorSaturation=1;
        projectorColorEnabled=true;
        projectorColorGui->getSlider("Red strength")->setValue(1);
        projectorColorGui->getSlider("Green strength")->setValue(1);
        projectorColorGui->getSlider("Blue strength")->setValue(1);
        projectorColorGui->getSlider("Color saturation")->setValue(1);
        projectorColorGui->getToggle("Correction enabled")->setChecked(true);
    }
    saveProjectorColor();
}

// EDGE_GUI_V6: desktop controls live around a clear center preview.
void ofApp::layoutOperatorGui(){
    if(!kinectProjector||!sandSurfaceRenderer)return;
    const float width=ofGetWidth(),height=ofGetHeight(),col=MagicSandOperator::columnWidth(width);
    // Set widths before measuring panel heights.
    sandSurfaceRenderer->placeOperatorGui(8,8,col);
    kinectProjector->placeOperatorSettings(0,0,col);kinectProjector->placeOperatorStatus(0,0,col);
    boidGameController.placeOperatorGui(0,0,col);triggerControls.placeOperatorGui(0,0,col);
    const float middleWidth=std::min(300.0f,std::max(1.0f,(width-2*col-40)*.5f));
    idleDemo.placeOperatorGui(0,0,middleWidth);cpuLimiter.placeOperatorGui(0,0,middleWidth);
    if(projectorColorGui)if(projectorColorGui->getWidth()!=col)projectorColorGui->setWidth(col);
    MagicSandOperator::Heights h;
    h.terrain=sandSurfaceRenderer->operatorGuiHeight();h.projector=projectorColorGui&&projectorColorVisible?projectorColorGui->getHeight():0;
    h.status=kinectProjector->operatorStatusHeight();h.camera=kinectProjector->operatorSettingsHeight();
    h.triggers=triggerControls.operatorGuiHeight();h.animals=boidGameController.operatorGuiHeight();
    h.demo=idleDemo.operatorGuiHeight();h.cooling=cpuLimiter.operatorGuiHeight();
    const bool camera=kinectProjector->GetApplicationState()!=KinectProjector::APPLICATION_STATE_RUNNING||kinectProjector->isCameraPreviewEnabled();
    float pw=camera?kinectProjector->getKinectRes().x:projectorComposite.getWidth(),ph=camera?kinectProjector->getKinectRes().y:projectorComposite.getHeight();
    auto l=MagicSandOperator::arrange(width,height,h,pw,ph,operatorLeftScroll,operatorRightScroll);
    operatorLeftMax=l.leftMax;operatorRightMax=l.rightMax;
    operatorLeftScroll=ofClamp(operatorLeftScroll,0,operatorLeftMax);operatorRightScroll=ofClamp(operatorRightScroll,0,operatorRightMax);
    sandSurfaceRenderer->placeOperatorGui(l.terrain.x,l.terrain.y,l.terrain.w);
    if(projectorColorGui)projectorColorGui->setPosition(l.projector.x,l.projector.y);
    kinectProjector->placeOperatorSettings(l.camera.x,l.camera.y,l.camera.w);
    kinectProjector->placeOperatorStatus(l.status.x,l.status.y,l.status.w);
    triggerControls.placeOperatorGui(l.triggers.x,l.triggers.y,l.triggers.w);
    boidGameController.placeOperatorGui(l.animals.x,l.animals.y,l.animals.w);
    idleDemo.placeOperatorGui(l.demo.x,l.demo.y,l.demo.w);cpuLimiter.placeOperatorGui(l.cooling.x,l.cooling.y,l.cooling.w);
    mainWindowROI=ofRectangle(l.preview.x,l.preview.y,l.preview.w,l.preview.h);
}
void ofApp::mouseScrolled(int x,int y,float scrollX,float scrollY){
    const bool shift=ofGetKeyPressed(OF_KEY_SHIFT);
    float col=MagicSandOperator::columnWidth(ofGetWidth());
    if(x<col+16 && (shift || !sandSurfaceRenderer || !sandSurfaceRenderer->operatorLayerListContains(x,y)))
        operatorLeftScroll=ofClamp(operatorLeftScroll-scrollY*45,0,operatorLeftMax);
    else if(shift && x>ofGetWidth()-col-16)
        operatorRightScroll=ofClamp(operatorRightScroll-scrollY*45,0,operatorRightMax);
    else return;
    layoutOperatorGui();
}

// BOX_SETTINGS_V7: startup snapshot is loaded once, not polled from disk.
void ofApp::loadBoxSettings(){
    try{boxSettingsAvailable=MagicSandBox::load(savedBoxSettings);
        if(boxSettingsAvailable){MagicSandModeButtons::restore(savedBoxSettings);boidGameController.refreshOperatorGui();sandSurfaceRenderer->restoreBoxGui(savedBoxSettings);boxCameraPending=true;kinectProjector->setBoxSaveStatus("Save box settings");}
    }catch(const std::exception& e){boxSettingsAvailable=false;boxCameraPending=false;kinectProjector->setBoxSaveStatus("Saved settings invalid");ofLogWarning("BoxSettings")<<e.what();}
}
bool ofApp::applyBoxAnimalDefaults(){
    if(!boxSettingsAvailable)return false;
    boidGameController.restoreBoxGui(savedBoxSettings);return true;
}
void ofApp::updateBoxSettings(){
    bool running=kinectProjector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING;
    // Native RUN resets sea level and loads camera settings. Apply the saved
    // operator profile once afterward so its tilt, offsets and ROI take effect.
    // Reapplying every frame would undo operator edits and reset depth filters.
    if(boxCameraPending&&running){
        try{kinectProjector->restoreBoxGui(savedBoxSettings);boxCameraPending=false;}
        catch(const std::exception& e){boxCameraPending=false;kinectProjector->setBoxSaveStatus("Settings restore failed");ofLogWarning("BoxSettings")<<e.what();}
    }
    // VISIBLE_BOX_ROI_V23: fit after saved startup geometry has been restored.
    // Refit only on ROI or calibration changes, once depth is stable.
    if(running&&kinectProjector->isImageStabilized()){
        static std::vector<float> fittedKey;
        auto key=[&](){std::vector<float> v;auto area=kinectProjector->getKinectROI();
            v={area.x,area.y,area.width,area.height};auto matrix=kinectProjector->getTransposedKinectProjMatrix();
            for(int row=0;row<4;++row){for(int col=0;col<4;++col){v.push_back(matrix(row,col));}}
            return v;};
        auto current=key();
        if(current!=fittedKey){kinectProjector->fitVisibleBoxROI();fittedKey=key();}
    }
    if(!kinectProjector->takeBoxSaveRequest())return;
    if(!running||fossilFinder.enabled||digSiteModeEnabled||!boidGameController.isIdle()||!mapGameController.isIdle()){
        kinectProjector->setBoxSaveStatus("Save in regular RUN");return;
    }
    try{
        ofJson j;j["box_schema"]=1;
        kinectProjector->captureBoxGui(j);sandSurfaceRenderer->captureBoxGui(j);boidGameController.captureBoxGui(j);
        MagicSandBox::validate(j);
        if(!kinectProjector->saveBoxCalibration()||!sandSurfaceRenderer->saveBoxNativeSettings())throw std::runtime_error("Unable to save calibration/display files");
        MagicSandBox::save(j);savedBoxSettings=j;boxSettingsAvailable=true;
        kinectProjector->setBoxSaveStatus("Saved box settings");
        std::cout<<"[BOX SETTINGS] saved: native controls, terrain colors and animal requests"<<std::endl;
    }catch(const std::exception& e){kinectProjector->setBoxSaveStatus("Save failed - see log");ofLogWarning("BoxSettings")<<e.what();}
}
