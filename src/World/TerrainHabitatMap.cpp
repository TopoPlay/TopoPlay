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
#include "../World/AnimalTerrain.h" // ANIMAL_LEVELS_V56
#include "TerrainHabitatMap.h"

#include <algorithm>
#include <cmath>
#include <queue>


TerrainHabitatMap::TerrainHabitatMap()
{
    sampleStep = 4;
    updateIntervalSeconds = 1.0f;
    lastUpdateTime = -1000.0f;

    ready = false;
    dirty = true;
    roiReady = false;

    gridWidth = 0;
    gridHeight = 0;
}


void TerrainHabitatMap::setup(
    const std::shared_ptr<KinectProjector>& projector)
{
    kinectProjector = projector;

    // Do NOT use KinectProjector's startup 512x424 ROI here.
    // The real sandbox ROI will be supplied later through setROI()
    // after calibration/settings have loaded.
    kinectROI.set(0, 0, 0, 0);
    roiReady = false;
    ready = false;

    invalidate();
}


void TerrainHabitatMap::setROI(const ofRectangle& roi)
{
    kinectROI = roi;

    // Kinect v2 startup temporarily reports the entire 512x424
    // depth frame as its ROI before the calibrated sandbox ROI loads.
    //
    // Do not allow the World system to analyze or spawn from that
    // temporary full-frame area.
    const bool startupFullFrame =
        kinectROI.x == 0 &&
        kinectROI.y == 0 &&
        kinectROI.width >= 511 &&
        kinectROI.height >= 423;

    roiReady =
        !startupFullFrame &&
        kinectROI.width > 0 &&
        kinectROI.height > 0;

    ready = false;
    invalidate();
}


void TerrainHabitatMap::setSampleStep(int pixels)
{
    sampleStep = std::max(1, pixels);
    invalidate();
}


void TerrainHabitatMap::setUpdateInterval(float seconds)
{
    updateIntervalSeconds = std::max(0.1f, seconds);
}


void TerrainHabitatMap::invalidate()
{
    dirty = true;
}


bool TerrainHabitatMap::isReady() const
{
    return ready;
}


const std::vector<HabitatCell>&
TerrainHabitatMap::getCells() const
{
    return cells;
}


const std::vector<HabitatRegion>&
TerrainHabitatMap::getRegions() const
{
    return regions;
}


const HabitatCell*
TerrainHabitatMap::getCellAt(
    const ofVec2f& position) const
{
    if (!ready ||
        gridWidth <= 0 ||
        gridHeight <= 0)
    {
        return nullptr;
    }

    if (!kinectROI.inside(position))
        return nullptr;

    int gx = static_cast<int>(
        (position.x - kinectROI.getLeft()) /
        sampleStep
    );

    int gy = static_cast<int>(
        (position.y - kinectROI.getTop()) /
        sampleStep
    );

    if (gx < 0 ||
        gy < 0 ||
        gx >= gridWidth ||
        gy >= gridHeight)
    {
        return nullptr;
    }

    int idx = cellIndex(gx, gy);

    if (idx < 0 ||
        idx >= static_cast<int>(cells.size()))
    {
        return nullptr;
    }

    return &cells[idx];
}


const ofRectangle&
TerrainHabitatMap::getROI() const
{
    return kinectROI;
}


int TerrainHabitatMap::getSampleStep() const
{
    return sampleStep;
}


int TerrainHabitatMap::cellIndex(int x, int y) const
{
    return y * gridWidth + x;
}


void TerrainHabitatMap::updateIfNeeded()
{
    if (!kinectProjector)
        return;

    // Never analyze the Kinect's temporary full-frame startup ROI.
    if (!roiReady)
        return;

    if (!kinectProjector->isImageStabilized())
        return;

    // A changed visual profile is not camera noise. Discard samples measured
    // in the previous scale instead of blending old and new water depths.
    unsigned revision=MagicSandAnimalTerrain::profile().revision;
    if(animalTerrainRevision!=revision){
        animalTerrainRevision=revision;cells.clear();regions.clear();dirty=true;
    }
    float now = ofGetElapsedTimef();

    if (!dirty &&
        now - lastUpdateTime < updateIntervalSeconds)
    {
        return;
    }

    rebuild();

    lastUpdateTime = now;
    dirty = false;
    ready = true;
}


void TerrainHabitatMap::rebuild()
{
    if (!roiReady ||
        kinectROI.width <= 0 ||
        kinectROI.height <= 0)
    {
        return;
    }

    // Preserve the previous terrain state. This allows the
    // habitat classifier to use temporal smoothing and hysteresis
    // instead of making an entirely new shoreline decision every
    // second.
    const std::vector<HabitatCell> previousCells = cells;

    const int previousGridWidth = gridWidth;
    const int previousGridHeight = gridHeight;


    gridWidth =
        std::max(
            1,
            static_cast<int>(
                std::ceil(kinectROI.width / sampleStep)
            )
        );

    gridHeight =
        std::max(
            1,
            static_cast<int>(
                std::ceil(kinectROI.height / sampleStep)
            )
        );


    const bool previousGridMatches =
        previousGridWidth == gridWidth &&
        previousGridHeight == gridHeight &&
        previousCells.size() ==
            static_cast<size_t>(
                gridWidth * gridHeight
            );


    cells.clear();
    cells.resize(gridWidth * gridHeight);

    for (int gy = 0; gy < gridHeight; gy++)
    {
        for (int gx = 0; gx < gridWidth; gx++)
        {
            float x =
                kinectROI.getLeft() +
                gx * sampleStep +
                sampleStep * 0.5f;

            float y =
                kinectROI.getTop() +
                gy * sampleStep +
                sampleStep * 0.5f;

            x = std::min(
                x,
                kinectROI.getRight() - 0.001f
            );

            y = std::min(
                y,
                kinectROI.getBottom() - 0.001f
            );

            HabitatCell& cell =
                cells[cellIndex(gx, gy)];

            cell.kinectPosition.set(x, y);

            cell.physicalElevation =
                kinectProjector->elevationAtKinectCoord(x,y);
            cell.rawElevation =
                MagicSandAnimalTerrain::elevation(cell.physicalElevation);


            const int idx =
                cellIndex(gx, gy);


            // ------------------------------------------------
            // Temporal terrain stabilization
            //
            // Habitat changes should react to real terrain
            // changes but should not chase millimeter-scale
            // Kinect noise every analysis cycle.
            // ------------------------------------------------
            if (previousGridMatches &&
                previousCells[idx].valid)
            {
                const float previousElevation =
                    previousCells[idx].elevation;

                // V78: stop tiny physical-height noise driving uphill tree
                // direction and material classification at the one-second tick.
                // A changed terrain profile still remaps the held physical value.
                if(std::abs(cell.physicalElevation-previousCells[idx].physicalElevation)<=2.f){
                    cell.physicalElevation=previousCells[idx].physicalElevation;
                    cell.rawElevation=MagicSandAnimalTerrain::elevation(cell.physicalElevation);
                }
                // 35% new measurement, 65% previous habitat
                // measurement.
                cell.elevation =
                    previousElevation * 0.65f +
                    cell.rawElevation * 0.35f;
                cell.physicalElevation=previousCells[idx].physicalElevation*0.65f+
                                       cell.physicalElevation*0.35f;
            }
            else
            {
                cell.elevation =
                    cell.rawElevation;
            }


            ofVec2f gradient =
                kinectProjector->
                    gradientAtKinectCoord(x, y);

            cell.slope =
                gradient.length();


            // ------------------------------------------------
            // Water/land hysteresis
            //
            // A previous water cell must rise substantially
            // above sea level before becoming land.
            //
            // A previous land cell must fall substantially
            // below sea level before becoming water.
            // ------------------------------------------------
            if (previousGridMatches)
            {
                if (previousCells[idx].water)
                {
                    cell.water =
                        cell.elevation < 2.0f;
                }
                else
                {
                    cell.water =
                        cell.elevation <= -2.0f;
                }
            }
            else
            {
                cell.water =
                    cell.elevation < 0.0f;
            }


            // ------------------------------------------------
            // Stable shoreline transition zone
            //
            // The color shader may blend beautifully through
            // this area, but ecology/spawning should not treat
            // it as dependable land or dependable water.
            //
            // +/- 3 mm around sea level is therefore excluded
            // from connected habitat regions.
            // ------------------------------------------------
            cell.shoreline =
                std::abs(cell.elevation) < 3.0f;

            cell.valid =
                !cell.shoreline;

            cell.regionId = -1;
        }
    }

    buildConnectedRegions();
}


void TerrainHabitatMap::buildConnectedRegions()
{
    regions.clear();

    if (cells.empty())
        return;

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    int nextRegionId = 0;

    for (int sy = 0; sy < gridHeight; sy++)
    {
        for (int sx = 0; sx < gridWidth; sx++)
        {
            int startIndex = cellIndex(sx, sy);

            if (!cells[startIndex].valid)
                continue;

            if (cells[startIndex].regionId >= 0)
                continue;

            HabitatRegion region;

            region.id = nextRegionId;
            region.water = cells[startIndex].water;

            region.minElevation =
                cells[startIndex].elevation;

            region.maxElevation =
                cells[startIndex].elevation;

            region.minSlope =
                cells[startIndex].slope;

            region.maxSlope =
                cells[startIndex].slope;

            double elevationTotal = 0.0;
            double slopeTotal = 0.0;

            std::queue<std::pair<int, int>> pending;

            cells[startIndex].regionId =
                nextRegionId;

            pending.push({sx, sy});

            while (!pending.empty())
            {
                int x = pending.front().first;
                int y = pending.front().second;
                pending.pop();

                int idx = cellIndex(x, y);

                HabitatCell& cell = cells[idx];

                region.cellIndices.push_back(idx);
                region.cellCount++;

                elevationTotal += cell.elevation;
                slopeTotal += cell.slope;

                region.minElevation =
                    std::min(
                        region.minElevation,
                        cell.elevation
                    );

                region.maxElevation =
                    std::max(
                        region.maxElevation,
                        cell.elevation
                    );

                region.minSlope =
                    std::min(
                        region.minSlope,
                        cell.slope
                    );

                region.maxSlope =
                    std::max(
                        region.maxSlope,
                        cell.slope
                    );

                for (int n = 0; n < 4; n++)
                {
                    int nx = x + dx[n];
                    int ny = y + dy[n];

                    if (nx < 0 ||
                        ny < 0 ||
                        nx >= gridWidth ||
                        ny >= gridHeight)
                    {
                        continue;
                    }

                    int nidx =
                        cellIndex(nx, ny);

                    HabitatCell& neighbor =
                        cells[nidx];

                    if (!neighbor.valid)
                        continue;

                    if (neighbor.regionId >= 0)
                        continue;

                    // For now, connected WATER forms one type
                    // of region and connected LAND another.
                    if (neighbor.water != region.water)
                        continue;

                    neighbor.regionId =
                        nextRegionId;

                    pending.push({nx, ny});
                }
            }

            region.area =
                region.cellCount *
                sampleStep *
                sampleStep;

            if (region.cellCount > 0)
            {
                region.averageElevation =
                    elevationTotal /
                    region.cellCount;

                region.averageSlope =
                    slopeTotal /
                    region.cellCount;
            }

            regions.push_back(region);

            nextRegionId++;
        }
    }
}
