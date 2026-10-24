/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#include "ProjectedModeButtons.h" // MODE_BUTTONS_V37
#include "BoneRoiFootprint.h" // BONE_CORNER_V36
#include "FossilRoiSurface.h" // FOSSIL_SURFACE_V35
#include "FossilBoxGeometry.h" // FOSSIL_ROI_V33
#include "AutoRunStartup.h" // AUTO_RUN_V32
/***********************************************************************
ofApp.h - main openframeworks app
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

#pragma once

#include "ofMain.h"
#include "BoxStartup.h"
#include "OperatorLayout.h"
#include "IdleDemo.h"
#include "CeilingMask.h" // IDLE_DEMO_V1
#include "TriggerControls.h" // TRIGGER_CONTROLS_V1
#include "CpuLimiter.h" // MAGICSAND_CPU_GUI_V1
#include "ofxDatGui.h"
#include "KinectProjector/KinectProjector.h"
#include "SandSurfaceRenderer/SandSurfaceRenderer.h"
#include "Games/MapGameController.h"
#include "Games/BoidGameController.h"
#include "World/RainRunoff.h"
#include "World/Volcano.h"

class ofApp : public ofBaseApp {
    CeilingMask ceilingMask; // CONTROLS_V3
    IdleDemo idleDemo; // IDLE_DEMO_V1

    TriggerControls triggerControls; // TRIGGER_CONTROLS_V1

    CpuLimiter cpuLimiter; // MAGICSAND_CPU_GUI_V1


public:
    void beginShutdown();
    bool shutdownComplete() const;
    void releaseShutdownGraphics();
	void setup();
 
	void update();
	void updateProjectorComposite();

	// Experimental projected control button.
	// Detects a physical raised ring placed around a known
	// projected button location.
	void updateRingButtonTest();
	void drawRingButtonTest();

	void draw();
	void drawProjWindow(ofEventArgs& args);

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
    void layoutOperatorGui();
    MagicSandAutoRun autoRunStartup; // AUTO_RUN_V32
    void loadBoxSettings();void updateBoxSettings();bool applyBoxAnimalDefaults();
    ofJson savedBoxSettings;bool boxSettingsAvailable=false,boxCameraPending=false; // BOX_SETTINGS_V7
    void mouseScrolled(int x,int y,float scrollX,float scrollY);
    float operatorLeftScroll=0,operatorRightScroll=0,operatorLeftMax=0,operatorRightMax=0; // EDGE_GUI_V6
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	std::shared_ptr<ofAppBaseWindow> projWindow;

private:
    bool shuttingDown=false;
    void setupProjectorColor();
    void drawCorrectedProjector();
    void saveProjectorColor();
    void onProjectorColorSlider(ofxDatGuiSliderEvent e);
    void onProjectorColorButton(ofxDatGuiButtonEvent e);
    void onProjectorColorToggle(ofxDatGuiToggleEvent e);
    std::unique_ptr<ofxDatGui> projectorColorGui;
    ofShader projectorColorShader;
    bool projectorColorReady=false,projectorColorVisible=true;
    bool projectorColorEnabled=true,projectorColorDirty=false;
    float projectorRed=1,projectorGreen=1,projectorBlue=1,projectorSaturation=1;
    float projectorColorChangedAt=0;
	std::shared_ptr<KinectProjector> kinectProjector;
	SandSurfaceRenderer* sandSurfaceRenderer;
    RainRunoff rainRunoff;
    Volcano volcano;
    bool volcanoFeatureEnabled=false;
    bool rainFeatureEnabled=false; // RAIN button starts OFF
	CMapGameController mapGameController;
	CBoidGameController boidGameController;

	// Main window ROI 
	ofRectangle mainWindowROI;

	// One finished image used by both projector and RUN preview
	ofFbo projectorComposite;

	// Window/update-rate diagnostic
	uint64_t diagUpdateCount = 0;
	uint64_t diagMainDrawCount = 0;
	uint64_t diagProjDrawCount = 0;
	float diagLastReport = 0.0f;

	// ============================================================
	// Projected finger-circle buttons
	// ============================================================

	struct RingButtonState
	{
		ofVec2f kinectCenter;

		float match = 0.0f;

		int matchedSections = 0;
		int consecutiveFrames = 0;
		int releaseFrames = 0;

		bool latched = false;
	};

	RingButtonState ringButtons[4];

	float ringButtonRadius = 26.0f;

	// Master switch for the buried-object / fossil system.
	bool digSiteModeEnabled = false;

	// ============================================================
	// Full-ROI Fossil Finder
	//
	// SAME excavation/reveal method as DIG mode, but:
	//
	//   * one large T-Rex
	//   * nearly the full ROI
	//   * no target circle
	//   * no red X
	//   * no rock rim
	//   * no chest
	//   * no GOOD JOB
	//   * no respawn
	// ============================================================

	struct FossilFinderState
	{
		bool enabled = false;

		float activatedTime = 0.0f;
		bool referenceFrozen = false;

		ofRectangle referenceROI;

		// Same concept as DIG:
		// one horizontal archaeological plane underneath the
		// frozen starting terrain.
		float objectPlaneElevation = 0.0f;

		// Large fossil orientation.
		float objectRotationDeg = 0.0f;

		bool objectPlacementReady = false;

		// Dense DIG-style reveal mask.
		ofMesh revealMaskMesh;
		float lastRevealMaskUpdate = -1000.0f;

		int revealEligibleCells = 0;
		int revealVisibleCells = 0;
		float actualRevealFraction = 0.0f;
	};

	FossilFinderState fossilFinder;
    MagicSandAutoBone::State digButtonLayout; // MODE_BUTTONS_V37
    FossilRoiSurface fossilRoiSurface; // FOSSIL_SURFACE_V35
    FossilBoxGeometry fossilGeometry; // FOSSIL_ROI_V33

	void startFossilFinder();
	void stopFossilFinder();
	void freezeFossilFinderReference();
	void updateFossilFinder();
	void drawFossilFinder();

	// ============================================================
	// Persistent excavation sites
	// ============================================================

	struct DigSiteState
	{
		ofVec2f position;

		std::vector<ofVec2f> sampleOffsets;
		std::vector<float> referenceElevations;

		// Current amount of sand removed at every sample point.
		// This becomes the reveal mask for fossils/treasure later.
		std::vector<float> excavationDepths;

		float excavationProgress = 0.0f;

        // Fixed archaeological plane underneath the sand.
        float objectPlaneElevation = 0.0f;

        // Random orientation of this buried object.
        float objectRotationDeg = 0.0f;

        bool objectPlacementReady = false;

        // Dense Kinect-resolution reveal mask.
        // Rebuilt only about 5 times/sec.
        ofMesh revealMaskMesh;
        float lastRevealMaskUpdate = -1000.0f;

        // Dense frozen starting surface for the DIG terrain.
        // One value per Kinect pixel inside the dig circle.
        std::vector<float> denseDigReferenceElevations;
        int denseDigRadius = 0;
        int denseDigDiameter = 0;

        // Batched projector-space mask for terrain that has
        // been excavated deeper than the original 5 mm crust.
        ofMesh digSandMaskMesh;
        float lastDigSandMaskUpdate = -1000.0f;

        // Actual visible-artifact completion tracking.
        //
        // Only nontransparent PNG cells count toward completion.
        int revealEligibleCells = 0;
        int revealVisibleCells = 0;
        float actualRevealFraction = 0.0f;

        // Once completed this remains latched even if sand
        // later falls back over part of the artifact.
        bool completionLatched = false;
        float completionTime = -1.0f;

        // Starts when this excavation first exceeds 5 mm.
        // Used to animate the rock rim outward from the center X.
        float rockRimStartTime = -1.0f;

		// 0 = T-Rex fossil
		// 1 = treasure chest
		int buriedObjectType = 0;

		bool referenceReady = false;
	};

	std::vector<DigSiteState> digSites;

	// About 12% of the shorter Kinect ROI dimension in radius.
	// That makes the full excavation zone roughly 24% across.
	float digSiteRadius = 50.0f;

	// Wait before freezing the terrain reference so the player's
	// hand/ring can leave the DIG control.
	float digModeActivatedTime = 0.0f;
	bool digReferenceFrozen = false;

	void createDigSites();
	bool respawnDigSite(size_t siteIndex);
	void freezeDigSiteReference();
	void updateDigSites();
	void drawDigSites();

	bool isInsideButtonNoDigZone(
		const ofVec2f& position
	) const;
};
