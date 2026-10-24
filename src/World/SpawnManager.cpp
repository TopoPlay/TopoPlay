#include "SpawnManager.h"

#include <algorithm>
#include <cmath>


SpawnManager::SpawnManager()
{
    habitatMap = nullptr;
}


void SpawnManager::setup(
    TerrainHabitatMap* map)
{
    habitatMap = map;
}


bool SpawnManager::cellMatchesProfile(
    const HabitatCell& cell,
    const SpawnProfile& profile) const
{
    if (!cell.valid)
        return false;

    if (profile.requireWater && !cell.water)
        return false;

    if (profile.requireLand && cell.water)
        return false;

    if (cell.elevation < profile.minElevation)
        return false;

    if (cell.elevation > profile.maxElevation)
        return false;

    if (cell.slope < profile.minSlope)
        return false;

    if (cell.slope > profile.maxSlope)
        return false;

    return true;
}


bool SpawnManager::regionMatchesProfile(
    const HabitatRegion& region,
    const SpawnProfile& profile) const
{
    if (profile.requireWater && !region.water)
        return false;

    if (profile.requireLand && region.water)
        return false;

    if (region.area < profile.minRegionArea)
        return false;

    return true;
}


std::vector<SpawnRegionInfo>
SpawnManager::getSuitableRegions(
    const SpawnProfile& profile) const
{
    std::vector<SpawnRegionInfo> result;

    if (!habitatMap ||
        !habitatMap->isReady())
    {
        return result;
    }

    const auto& regions =
        habitatMap->getRegions();

    const auto& cells =
        habitatMap->getCells();

    float cellArea =
        habitatMap->getSampleStep() *
        habitatMap->getSampleStep();

    for (const HabitatRegion& region : regions)
    {
        if (!regionMatchesProfile(region, profile))
            continue;

        int suitableCellCount = 0;

        for (int idx : region.cellIndices)
        {
            if (idx < 0 ||
                idx >= static_cast<int>(cells.size()))
            {
                continue;
            }

            if (cellMatchesProfile(
                    cells[idx],
                    profile))
            {
                suitableCellCount++;
            }
        }

        float usableArea =
            suitableCellCount * cellArea;

        if (usableArea < profile.minRegionArea)
            continue;

        SpawnRegionInfo info;

        info.regionId = region.id;
        info.usableArea = usableArea;

        if (profile.areaPerObject > 0.0f)
        {
            info.capacity =
                static_cast<int>(
                    std::floor(
                        usableArea /
                        profile.areaPerObject
                    )
                );
        }
        else
        {
            info.capacity =
                suitableCellCount;
        }

        if (info.capacity > 0)
            result.push_back(info);
    }

    return result;
}


int SpawnManager::getCapacity(
    const SpawnProfile& profile) const
{
    if (profile.areaPerObject <= 0.0f)
    {
        // No habitat-based population limit requested.
        return std::numeric_limits<int>::max();
    }

    int total = 0;

    auto regions =
        getSuitableRegions(profile);

    for (const auto& region : regions)
        total += region.capacity;

    return total;
}


int SpawnManager::getTargetPopulation(
    const SpawnProfile& profile,
    int requestedCount) const
{
    if (requestedCount <= 0)
        return 0;

    if (profile.areaPerObject <= 0.0f)
        return requestedCount;

    return std::min(
        requestedCount,
        getCapacity(profile)
    );
}


bool SpawnManager::isLocationSuitable(
    const SpawnProfile& profile,
    const ofVec2f& location) const
{
    if (!habitatMap ||
        !habitatMap->isReady())
    {
        return false;
    }

    const HabitatCell* cell =
        habitatMap->getCellAt(location);

    if (!cell)
        return false;

    if (!cellMatchesProfile(
            *cell,
            profile))
    {
        return false;
    }

    // The cell itself may match while its connected region is too
    // small for this profile, so confirm the region also qualifies.
    const auto& regions =
        habitatMap->getRegions();

    for (const auto& region : regions)
    {
        if (region.id != cell->regionId)
            continue;

        return regionMatchesProfile(
            region,
            profile
        );
    }

    return false;
}


bool SpawnManager::findSpawnLocation(
    const SpawnProfile& profile,
    ofVec2f& location) const
{
    auto suitableRegions =
        getSuitableRegions(profile);

    if (suitableRegions.empty())
        return false;

    float totalArea = 0.0f;

    for (const auto& region : suitableRegions)
        totalArea += region.usableArea;

    if (totalArea <= 0.0f)
        return false;

    // Weighted selection:
    // a region with twice as much usable habitat is
    // twice as likely to receive the next spawn.
    float pick = ofRandom(totalArea);

    int chosenRegion =
        suitableRegions.back().regionId;

    float running = 0.0f;

    for (const auto& region : suitableRegions)
    {
        running += region.usableArea;

        if (pick <= running)
        {
            chosenRegion = region.regionId;
            break;
        }
    }

    return findSpawnLocationInRegion(
        profile,
        chosenRegion,
        location
    );
}


bool SpawnManager::findBalancedSpawnLocation(
    const SpawnProfile& profile,
    const std::vector<ofVec2f>& existingPositions,
    ofVec2f& location) const
{
    auto regions =
        getSuitableRegions(profile);

    if (regions.empty())
        return false;

    if (!habitatMap ||
        !habitatMap->isReady())
    {
        return false;
    }

    std::vector<int> counts(
        regions.size(),
        0
    );

    int validExistingCount = 0;

    // Count existing objects in each suitable region.
    for (const ofVec2f& position : existingPositions)
    {
        const HabitatCell* cell =
            habitatMap->getCellAt(position);

        if (!cell)
            continue;

        if (!cellMatchesProfile(
                *cell,
                profile))
        {
            continue;
        }

        for (size_t i = 0;
             i < regions.size();
             ++i)
        {
            if (regions[i].regionId ==
                cell->regionId)
            {
                counts[i]++;
                validExistingCount++;
                break;
            }
        }
    }

    float totalUsableArea = 0.0f;

    for (const auto& region : regions)
        totalUsableArea += region.usableArea;

    if (totalUsableArea <= 0.0f)
        return false;

    const int nextPopulation =
        validExistingCount + 1;

    int bestIndex = -1;
    float bestDeficit = -1.0e30f;

    for (size_t i = 0;
         i < regions.size();
         ++i)
    {
        // Never deliberately exceed this region's habitat capacity.
        if (profile.areaPerObject > 0.0f &&
            counts[i] >= regions[i].capacity)
        {
            continue;
        }

        float desiredPopulation =
            nextPopulation *
            (regions[i].usableArea /
             totalUsableArea);

        float deficit =
            desiredPopulation -
            counts[i];

        if (bestIndex < 0 ||
            deficit > bestDeficit)
        {
            bestIndex =
                static_cast<int>(i);

            bestDeficit = deficit;
        }
    }

    if (bestIndex < 0)
        return false;

    return findSpawnLocationInRegion(
        profile,
        regions[bestIndex].regionId,
        location
    );
}


bool SpawnManager::findSpawnLocationInRegion(
    const SpawnProfile& profile,
    int regionId,
    ofVec2f& location) const
{
    if (!habitatMap ||
        !habitatMap->isReady())
    {
        return false;
    }

    const auto& regions =
        habitatMap->getRegions();

    const auto& cells =
        habitatMap->getCells();

    const HabitatRegion* targetRegion = nullptr;

    for (const auto& region : regions)
    {
        if (region.id == regionId)
        {
            targetRegion = &region;
            break;
        }
    }

    if (!targetRegion)
        return false;

    std::vector<int> candidates;

    candidates.reserve(
        targetRegion->cellIndices.size()
    );

    for (int idx : targetRegion->cellIndices)
    {
        if (idx < 0 ||
            idx >= static_cast<int>(cells.size()))
        {
            continue;
        }

        if (cellMatchesProfile(
                cells[idx],
                profile))
        {
            candidates.push_back(idx);
        }
    }

    if (candidates.empty())
        return false;

    int chosen =
        candidates[
            static_cast<int>(
                ofRandom(
                    static_cast<float>(
                        candidates.size()
                    )
                )
            ) %
            candidates.size()
        ];

    location =
        cells[chosen].kinectPosition;

    // Scatter within the coarse sample cell so objects don't
    // appear locked onto a visible grid.
    float halfStep =
        habitatMap->getSampleStep() * 0.45f;

    location.x +=
        ofRandom(-halfStep, halfStep);

    location.y +=
        ofRandom(-halfStep, halfStep);

    return true;
}
