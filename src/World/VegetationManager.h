#pragma once

#include "ofMain.h"
#include "TreeFire.h"

#include "EnvironmentFeature.h"
#include "TerrainHabitatMap.h"
#include "SpawnManager.h"

#include "../KinectProjector/KinectProjector.h"

#include <memory>
#include <vector>


class VegetationManager
{
public:

    VegetationManager();

    void setup(
        TerrainHabitatMap* habitatMap,
        SpawnManager* spawnManager
    );

    void updateIfNeeded();

    void draw(
        const std::shared_ptr<KinectProjector>& kinectProjector
    ) const;


    // ------------------------------------------------------------
    // Generic habitat queries for animals and future systems.
    // ------------------------------------------------------------

    bool hasFeatureNear(
        EnvironmentFeatureType type,
        const ofVec2f& position,
        float radius
    ) const;

    int countFeaturesNear(
        EnvironmentFeatureType type,
        const ofVec2f& position,
        float radius
    ) const;


    // Generic environmental habitat queries.
    // These know nothing about rabbits or any other animal.
    bool hasAnyFeatureNear(
        const std::vector<EnvironmentFeatureType>& types,
        const ofVec2f& position,
        float radius,
        int minimumFeatureCount = 1
    ) const;

    int countFeaturesOfTypes(
        const std::vector<EnvironmentFeatureType>& types
    ) const;

    // Find the nearest matching environmental feature.
    // Returns false if none exists within maxRadius.
    bool findNearestFeature(
        const std::vector<EnvironmentFeatureType>& types,
        const ofVec2f& position,
        float maxRadius,
        ofVec2f& featurePosition,
        float& distance
    ) const;

    int getFeatureSupportCapacity(
        const std::vector<EnvironmentFeatureType>& types,
        int featuresPerObject
    ) const;

    bool findSpawnLocationNearFeatures(
        const SpawnProfile& terrainProfile,
        const std::vector<EnvironmentFeatureType>& requiredFeatures,
        float featureRadius,
        int minimumFeatureCount,
        const std::vector<ofVec2f>& existingPositions,
        ofVec2f& location
    ) const;


    const std::vector<EnvironmentFeature>&
    getFeatures() const
    {
        return features;
    }


private:

    TerrainHabitatMap* habitatMap;
    SpawnManager* spawnManager;

    std::vector<EnvironmentFeature> features;

    ofImage pineImage;
    bool pineLoaded=false;
    mutable ofVboMesh pineMesh;
    mutable float nextPineMesh=0;
    struct PineDirection {ofVec2f position;float angle;MagicSandTreeFire::Burn burn;ofVec2f foot;float radius;};
    mutable unsigned fireGeneration=0;
    mutable ofVboMesh treeFireMesh;
    mutable float nextTreeFireMesh=0;
    struct BurningPine {ofVec2f foot,up;float size,seed,started;};
    mutable std::vector<BurningPine> burningPines;
    void rebuildTreeFireMesh(float now) const;
    mutable std::vector<PineDirection> pineDirections;
    void rebuildPineMesh(const std::shared_ptr<KinectProjector>& projector) const;


    // ------------------------------------------------------------
    // Habitat profiles
    // ------------------------------------------------------------

    SpawnProfile treeProfile;
    SpawnProfile grassProfile;
    SpawnProfile shrubProfile;
    SpawnProfile flowerProfile;


    // Maximum requested populations.
    // Actual populations are limited by terrain capacity.
    int requestedTreeCount;
    int requestedGrassCount;
    int requestedShrubCount;
    int requestedFlowerCount;


    float lastUpdateTime;
    float updateInterval;


    void updateFeatureType(
        EnvironmentFeatureType type,
        const SpawnProfile& profile,
        int requestedCount,
        float minSize,
        float maxSize,
        const std::vector<EnvironmentFeatureType>&
            requiredNearbyTypes = {},
        float nearbyRadius = 0.0f,
        int minimumNearbyCount = 0
    );

    bool addFeature(
        EnvironmentFeatureType type,
        const SpawnProfile& profile,
        float minSize,
        float maxSize,
        const std::vector<EnvironmentFeatureType>&
            requiredNearbyTypes = {},
        float nearbyRadius = 0.0f,
        int minimumNearbyCount = 0
    );

    bool spacingIsValid(
        EnvironmentFeatureType type,
        const ofVec2f& candidate,
        float minimumSpacing
    ) const;

    int countType(
        EnvironmentFeatureType type
    ) const;

    void removeLastOfType(
        EnvironmentFeatureType type
    );

    std::vector<ofVec2f>
    positionsOfType(
        EnvironmentFeatureType type
    ) const;


    void drawTree(
        const EnvironmentFeature& feature,
        const std::shared_ptr<KinectProjector>& projector
    ) const;

    void drawGrass(
        const EnvironmentFeature& feature,
        const std::shared_ptr<KinectProjector>& projector
    ) const;

    void drawShrub(
        const EnvironmentFeature& feature,
        const std::shared_ptr<KinectProjector>& projector
    ) const;

    void drawFlower(
        const EnvironmentFeature& feature,
        const std::shared_ptr<KinectProjector>& projector
    ) const;
};
