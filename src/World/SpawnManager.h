#pragma once

#include "ofMain.h"
#include "TerrainHabitatMap.h"
#include "SpawnProfile.h"

#include <vector>


struct SpawnRegionInfo
{
    int regionId = -1;

    float usableArea = 0.0f;

    int capacity = 0;
};


class SpawnManager
{
public:
    SpawnManager();

    void setup(TerrainHabitatMap* habitatMap);

    // Returns terrain regions compatible with this profile.
    std::vector<SpawnRegionInfo>
    getSuitableRegions(
        const SpawnProfile& profile
    ) const;

    // Maximum population supported by currently suitable habitat.
    //
    // If profile.areaPerObject <= 0, habitat area does not impose
    // a population ceiling.
    int getCapacity(
        const SpawnProfile& profile
    ) const;

    // Requested count remains the user's/configuration target.
    // Actual target may be lower if habitat cannot support it.
    int getTargetPopulation(
        const SpawnProfile& profile,
        int requestedCount
    ) const;

    // Find a valid location in any suitable region.
    bool findSpawnLocation(
        const SpawnProfile& profile,
        ofVec2f& location
    ) const;

    // Test whether an existing object's current position still
    // satisfies a SpawnProfile against the latest habitat map.
    //
    // Useful for vegetation, terrain-bound objects and animals
    // whose habitat can change underneath them.
    bool isLocationSuitable(
        const SpawnProfile& profile,
        const ofVec2f& location
    ) const;

    // Balanced version.
    //
    // Existing positions are counted within suitable habitat
    // regions. The next spawn goes preferentially into the region
    // that is furthest below its area-proportional share.
    //
    // This is completely generic: fish, sharks, trees, flowers,
    // bones, etc. can all use the same function.
    bool findBalancedSpawnLocation(
        const SpawnProfile& profile,
        const std::vector<ofVec2f>& existingPositions,
        ofVec2f& location
    ) const;

    // Same query, constrained to a specific connected region.
    bool findSpawnLocationInRegion(
        const SpawnProfile& profile,
        int regionId,
        ofVec2f& location
    ) const;

private:
    TerrainHabitatMap* habitatMap;

    bool cellMatchesProfile(
        const HabitatCell& cell,
        const SpawnProfile& profile
    ) const;

    bool regionMatchesProfile(
        const HabitatRegion& region,
        const SpawnProfile& profile
    ) const;
};
