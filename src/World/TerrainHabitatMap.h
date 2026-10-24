/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ofMain.h"
#include "../KinectProjector/KinectProjector.h"

#include <memory>
#include <vector>

// One sampled terrain point.
//
// TerrainHabitatMap intentionally knows nothing about fish, sharks,
// rabbits, trees, flowers, etc.
struct HabitatCell
{
    ofVec2f kinectPosition;

    // Current habitat elevation, normalized to the displayed terrain bands.
    float rawElevation = 0.0f;

    // Unwarped stabilized height for geometric tree orientation.
    float physicalElevation = 0.0f;

    // Temporally stabilized habitat elevation used by the World system.
    float elevation = 0.0f;

    float slope = 0.0f;

    bool water = false;

    // True when the cell is in the deliberately excluded
    // water/land transition band.
    bool shoreline = false;

    bool valid = false;

    int regionId = -1;
};


// Connected region of similar basic habitat.
//
// For the first implementation, regions are separated primarily into
// connected WATER and LAND areas. Additional classifications can be
// layered on top later without changing object/game code.
struct HabitatRegion
{
    int id = -1;

    bool water = false;

    int cellCount = 0;

    // Approximate Kinect-pixel area represented by this region.
    float area = 0.0f;

    float minElevation = 0.0f;
    float maxElevation = 0.0f;
    float averageElevation = 0.0f;

    float minSlope = 0.0f;
    float maxSlope = 0.0f;
    float averageSlope = 0.0f;

    std::vector<int> cellIndices;
};


class TerrainHabitatMap
{
public:
    TerrainHabitatMap();

    void setup(
        const std::shared_ptr<KinectProjector>& projector
    );

    void setROI(const ofRectangle& roi);

    // Sampling interval in Kinect pixels.
    // Larger values reduce CPU cost.
    void setSampleStep(int pixels);

    // Minimum time between complete terrain analyses.
    void setUpdateInterval(float seconds);

    // Does nothing unless the update interval has elapsed.
    void updateIfNeeded();

    // Force an immediate rebuild next time updateIfNeeded() is called.
    void invalidate();

    bool isReady() const;

    const std::vector<HabitatCell>& getCells() const;
    const std::vector<HabitatRegion>& getRegions() const;

    // Return the cached habitat cell containing a Kinect position.
    // nullptr means the position is outside the analyzed sandbox.
    const HabitatCell* getCellAt(
        const ofVec2f& position
    ) const;

    const ofRectangle& getROI() const;

    int getSampleStep() const;

private:
    std::shared_ptr<KinectProjector> kinectProjector;

    ofRectangle kinectROI;

    int sampleStep;
    float updateIntervalSeconds;
    float lastUpdateTime;
    unsigned animalTerrainRevision=0;

    bool ready;
    bool dirty;

    // Becomes true only after the application supplies the
    // calibrated sandbox ROI via setROI().
    bool roiReady;

    int gridWidth;
    int gridHeight;

    std::vector<HabitatCell> cells;
    std::vector<HabitatRegion> regions;

    void rebuild();
    void buildConnectedRegions();

    int cellIndex(int x, int y) const;
};
