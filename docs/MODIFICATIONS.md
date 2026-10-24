# TopoPlay source modification record

Notice update: 2026-10-23. The initial public source snapshot entered the repository
on October 23, 2026. Earlier local development dates were not individually recorded;
this record does not invent or backdate them. Each marked file is modified again
by this notice update. Existing upstream copyrights and license grants remain.

The comparison reference is Magic-Sand upstream commit
`0d26ba89d111884cd92bd517db6eff7f4ffb4c8b` from
https://github.com/thomwolf/Magic-Sand . It is a comparison reference, not a claim
that this was the exact historical base of every local change. Matching temporal
filter files and existing dlib headers are left unchanged.

Changes include terrain rendering/stability, Kinect v2 integration, operator and
projector controls, wildlife, rain/pooling/waves, volcanoes, fossils/dig, saved
settings, startup/shutdown and the TopoPlay name. Older feature notes and inline
source markers provide further detail. No original author's contribution is
reassigned to TopoPlay. New project source uses GPL-2.0-or-later except separately
identified components such as the MIT-adapted ripple algorithm.

## Marked files

| File | Relationship to comparison source |
| --- | --- |
| `src/AutoBonePlacement.h` | Added in the TopoPlay educational fork |
| `src/AutoRunStartup.h` | Added in the TopoPlay educational fork |
| `src/BoneButton.h` | Added in the TopoPlay educational fork |
| `src/BoneRoiFootprint.h` | Added in the TopoPlay educational fork |
| `src/BoxStartup.h` | Added in the TopoPlay educational fork |
| `src/CeilingDepth.h` | Added in the TopoPlay educational fork |
| `src/CeilingMask.h` | Added in the TopoPlay educational fork |
| `src/CpuLimiter.h` | Added in the TopoPlay educational fork |
| `src/FlexibleRing.h` | Added in the TopoPlay educational fork |
| `src/FossilBoxGeometry.h` | Added in the TopoPlay educational fork |
| `src/FossilRoiSurface.h` | Added in the TopoPlay educational fork |
| `src/Games/BoidGameController.cpp` | Modified from upstream Magic-Sand |
| `src/Games/BoidGameController.h` | Modified from upstream Magic-Sand |
| `src/Games/MapGameController.cpp` | Modified from upstream Magic-Sand |
| `src/Games/MapGameController.h` | Modified from upstream Magic-Sand |
| `src/Games/ReferenceMapHandler.cpp` | Modified from upstream Magic-Sand |
| `src/Games/ReferenceMapHandler.h` | Modified from upstream Magic-Sand |
| `src/Games/SandboxScoreTracker.cpp` | Modified from upstream Magic-Sand |
| `src/Games/SandboxScoreTracker.h` | Modified from upstream Magic-Sand |
| `src/Games/vehicle.cpp` | Modified from upstream Magic-Sand |
| `src/Games/vehicle.h` | Modified from upstream Magic-Sand |
| `src/IdleDemo.h` | Added in the TopoPlay educational fork |
| `src/KinectProjector/KinectGrabber.cpp` | Modified from upstream Magic-Sand |
| `src/KinectProjector/KinectGrabber.h` | Modified from upstream Magic-Sand |
| `src/KinectProjector/KinectProjector.cpp` | Modified from upstream Magic-Sand |
| `src/KinectProjector/KinectProjector.h` | Modified from upstream Magic-Sand |
| `src/KinectProjector/KinectProjectorCalibration.cpp` | Modified from upstream Magic-Sand |
| `src/KinectProjector/KinectProjectorCalibration.h` | Modified from upstream Magic-Sand |
| `src/KinectProjector/Utils.h` | Modified from upstream Magic-Sand |
| `src/LengthUnits.h` | Added in the TopoPlay educational fork |
| `src/ModeButtonSettings.h` | Added in the TopoPlay educational fork |
| `src/OperatorLayout.h` | Added in the TopoPlay educational fork |
| `src/OperatorPanelScroll.h` | Added in the TopoPlay educational fork |
| `src/ProjectedModeButtons.h` | Added in the TopoPlay educational fork |
| `src/ProjectorDisplayGuard.h` | Added in the TopoPlay educational fork |
| `src/SandSurfaceRenderer/ColorMap.cpp` | Modified from upstream Magic-Sand |
| `src/SandSurfaceRenderer/ColorMap.h` | Modified from upstream Magic-Sand |
| `src/SandSurfaceRenderer/LinkedTerrainLayers.h` | Added in the TopoPlay educational fork |
| `src/SandSurfaceRenderer/RealisticTerrainLayers.h` | Added in the TopoPlay educational fork |
| `src/SandSurfaceRenderer/SandSurfaceRenderer.cpp` | Modified from upstream Magic-Sand |
| `src/SandSurfaceRenderer/SandSurfaceRenderer.h` | Modified from upstream Magic-Sand |
| `src/SandSurfaceRenderer/TerrainTextureList.h` | Added in the TopoPlay educational fork |
| `src/StableTerrainHeight.h` | Added in the TopoPlay educational fork |
| `src/TopoPlayShutdown.h` | Added in the TopoPlay educational fork |
| `src/TriggerControls.h` | Added in the TopoPlay educational fork |
| `src/TriggerSettings.h` | Added in the TopoPlay educational fork |
| `src/VisibleBoxROI.h` | Added in the TopoPlay educational fork |
| `src/World/AmbientWaterRipples.h` | Added in the TopoPlay educational fork |
| `src/World/AnimalTerrain.h` | Added in the TopoPlay educational fork |
| `src/World/EnvironmentFeature.h` | Added in the TopoPlay educational fork |
| `src/World/InteractiveWater.h` | Added in the TopoPlay educational fork |
| `src/World/LavaCover.h` | Added in the TopoPlay educational fork |
| `src/World/LavaSurfaceMotion.h` | Added in the TopoPlay educational fork |
| `src/World/RainPoolShimmer.h` | Added in the TopoPlay educational fork |
| `src/World/RainRunoff.cpp` | Added in the TopoPlay educational fork |
| `src/World/RainRunoff.h` | Added in the TopoPlay educational fork |
| `src/World/RainSurfaceLook.h` | Added in the TopoPlay educational fork |
| `src/World/RainWaterGrid.h` | Added in the TopoPlay educational fork |
| `src/World/SpawnManager.cpp` | Added in the TopoPlay educational fork |
| `src/World/SpawnManager.h` | Added in the TopoPlay educational fork |
| `src/World/SpawnProfile.h` | Added in the TopoPlay educational fork |
| `src/World/StormCloud.h` | Added in the TopoPlay educational fork |
| `src/World/TerrainHabitatMap.cpp` | Added in the TopoPlay educational fork |
| `src/World/TerrainHabitatMap.h` | Added in the TopoPlay educational fork |
| `src/World/TreeFire.h` | Added in the TopoPlay educational fork |
| `src/World/VegetationManager.cpp` | Added in the TopoPlay educational fork |
| `src/World/VegetationManager.h` | Added in the TopoPlay educational fork |
| `src/World/Volcano.cpp` | Added in the TopoPlay educational fork |
| `src/World/Volcano.h` | Added in the TopoPlay educational fork |
| `src/World/VolcanoGrid.h` | Added in the TopoPlay educational fork |
| `src/World/WaterRippleGrid.h` | Added in the TopoPlay educational fork |
| `src/World/WaterRippleSettings.h` | Added in the TopoPlay educational fork |
| `src/World/WaterSweep.h` | Added in the TopoPlay educational fork |
| `src/main.cpp` | Modified from upstream Magic-Sand |
| `src/ofApp.cpp` | Modified from upstream Magic-Sand |
| `src/ofApp.h` | Modified from upstream Magic-Sand |
| `bin/data/shaders/shadersGL2/heightMapShader.frag` | Modified shader |
| `bin/data/shaders/shadersGL2/heightMapShader.vert` | Modified shader |
| `bin/data/shaders/shadersGL3/heightMapShader.frag` | Modified shader |
| `bin/data/shaders/shadersGL3/heightMapShader.vert` | Modified shader |
| `dependencies/tested-addon-sources/ofxDatGui/src/components/ofxDatGuiSlider.h` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxDatGui/src/components/ofxDatGuiTextInputField.h` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxDatGui/src/core/ofxDatGuiComponent.cpp` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxDatGui/src/core/ofxDatGuiComponent.h` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxDatGui/src/ofxDatGui.h` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxModal/src/ofxModalWindow.cpp` | Captured addon compatibility modification |
| `dependencies/tested-addon-sources/ofxModal/src/ofxModalWindow.h` | Captured addon compatibility modification |

## Other changes in this source-release update

- Include the exact openFrameworks 0.12.1 license and addon MIT notices.
- Record byte-matched ofxDatGui GUI image provenance; artwork is unchanged.
- Retain the credited original thunder recording and its supplied permission record;
  generated audio is an optional alternative. An intermediate substitution is reverted.
- Make source-only distribution explicit and refresh asset/package checksums.
- Source annotations are comments only; simulation and rendering logic are unchanged.

## Fresh calibration update — 2026-10-24

The Kinect/projector acquisition and calibration implementation was modified to
clear attempts, wait for fresh frames, resolve corner ordering, normalize the
projective fit, enforce two-height/error checks, and preserve compatible saved
calibration coefficients. `FreshCalibration.h` and `FreshCaptureGate.h` are new
GPL-2.0-or-later helpers. The rendering files were restored to the composition
used in the successful calibration tests; responsive shutdown is retained.
`ofApp.cpp` also includes opt-in calibration tracing. Original source notices and
licenses remain in place. README calibration instructions were corrected.
