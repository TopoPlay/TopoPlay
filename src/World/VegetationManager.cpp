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
#include "AnimalTerrain.h" // ANIMAL_LEVELS_V56
/* PROJECT_ATTRIBUTION_V45
 * Pine sprite created with ChatGPT image generation during this fork's
 * development; see ASSETS.md. Existing source notices remain in place.
 * Procedural fire and placement refinements were developed with ChatGPT/Codex.
 */
#include "VegetationManager.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>


VegetationManager::VegetationManager()
:
    habitatMap(nullptr),
    spawnManager(nullptr),

    requestedTreeCount(30),
    requestedGrassCount(70),
    requestedShrubCount(25),
    requestedFlowerCount(45),

    lastUpdateTime(-1000.0f),
    updateInterval(2.0f)
{
}


void VegetationManager::setup(
    TerrainHabitatMap* habitat,
    SpawnManager* spawner)
{
    habitatMap = habitat;
    spawnManager = spawner;
    pineLoaded=pineImage.load("vegetation/pine-v1.png");
    nextPineMesh=0;pineDirections.clear();pineMesh.clear();
    fireGeneration=MagicSandTreeFire::contactMap().generation;
    treeFireMesh.clear();burningPines.clear();nextTreeFireMesh=0;
    std::cout<<"[PINE TREES] sprite "<<(pineLoaded?"loaded":"MISSING; using old tree fallback")<<std::endl;


    // ============================================================
    // TREES
    // ============================================================

    treeProfile.name = "tree";

    treeProfile.requireWater = false;
    treeProfile.requireLand = true;

    treeProfile.minElevation = 2.0f;
    treeProfile.maxElevation =
        std::numeric_limits<float>::max();

    treeProfile.minSlope = 0.0f;
    treeProfile.maxSlope = 3.0f;

    treeProfile.minRegionArea = 1000.0f;
    treeProfile.areaPerObject = 1200.0f;

    treeProfile.minSpacing = 14.0f;


    // ============================================================
    // GRASS
    //
    // Grass prefers broad, fairly gentle land.
    // This will eventually be one of the main rabbit habitat
    // features.
    // ============================================================

    grassProfile.name = "grass";

    grassProfile.requireWater = false;
    grassProfile.requireLand = true;

    grassProfile.minElevation = 1.0f;
    grassProfile.maxElevation =
        std::numeric_limits<float>::max();

    grassProfile.minSlope = 0.0f;
    grassProfile.maxSlope = 2.2f;

    grassProfile.minRegionArea = 500.0f;

    // Denser than trees.
    grassProfile.areaPerObject = 420.0f;

    grassProfile.minSpacing = 7.0f;


    // ============================================================
    // SHRUBS
    //
    // Shrubs tolerate somewhat rougher terrain than grass but
    // should remain less dense.
    // ============================================================

    shrubProfile.name = "shrub";

    shrubProfile.requireWater = false;
    shrubProfile.requireLand = true;

    shrubProfile.minElevation = 1.0f;
    shrubProfile.maxElevation =
        std::numeric_limits<float>::max();

    shrubProfile.minSlope = 0.0f;
    shrubProfile.maxSlope = 2.8f;

    shrubProfile.minRegionArea = 700.0f;

    shrubProfile.areaPerObject = 1500.0f;

    shrubProfile.minSpacing = 12.0f;


    // ============================================================
    // FLOWERS
    //
    // Flowers prefer gentle, established land rather than bare
    // shoreline or steep peaks.
    //
    // Terrain controls the broad physical habitat.
    // A second generic environmental rule below requires nearby
    // grass or shrubs.
    // ============================================================

    flowerProfile.name = "flower";

    flowerProfile.requireWater = false;
    flowerProfile.requireLand = true;

    // Avoid the immediate shoreline.
    flowerProfile.minElevation = 3.0f;

    // Favor lower/mid terrain instead of the highest peaks.
    flowerProfile.maxElevation = 45.0f;

    flowerProfile.minSlope = 0.0f;
    flowerProfile.maxSlope = 2.0f;

    flowerProfile.minRegionArea = 500.0f;

    // Fairly dense but still visibly separated.
    flowerProfile.areaPerObject = 500.0f;

    flowerProfile.minSpacing = 6.0f;
}


void VegetationManager::updateIfNeeded()
{
    if (!habitatMap ||
        !spawnManager ||
        !habitatMap->isReady())
    {
        return;
    }

    float now = ofGetElapsedTimef();

    if (now - lastUpdateTime < updateInterval)
        return;

    lastUpdateTime = now;

    // Plant records follow the same material scale as the texture shader.
    // Keep the existing two-second budget; sliders must not trigger an expensive
    // plant respawn pass every frame. Bare beach, rock and snow are not forest.
    if(MagicSandAnimalTerrain::profile().enabled){
        treeProfile.minElevation=15.0f;treeProfile.maxElevation=134.0f;
        grassProfile.minElevation=15.0f;grassProfile.maxElevation=96.0f;
        shrubProfile.minElevation=15.0f;shrubProfile.maxElevation=134.0f;
        flowerProfile.minElevation=15.0f;flowerProfile.maxElevation=65.0f;
    }else{
        float unbounded=std::numeric_limits<float>::max();
        treeProfile.minElevation=2.0f;treeProfile.maxElevation=unbounded;
        grassProfile.minElevation=1.0f;grassProfile.maxElevation=unbounded;
        shrubProfile.minElevation=1.0f;shrubProfile.maxElevation=unbounded;
        flowerProfile.minElevation=3.0f;flowerProfile.maxElevation=45.0f;
    }



    updateFeatureType(
        EnvironmentFeatureType::Tree,
        treeProfile,
        requestedTreeCount,
        3.5f,
        5.5f
    );

    updateFeatureType(
        EnvironmentFeatureType::Grass,
        grassProfile,
        requestedGrassCount,
        2.0f,
        3.5f
    );

    updateFeatureType(
        EnvironmentFeatureType::Shrub,
        shrubProfile,
        requestedShrubCount,
        2.8f,
        4.4f
    );


    // Flowers depend on an already-established vegetation layer.
    // This is a generic dependency mechanism, not a flower-only
    // spawning system.
    std::vector<EnvironmentFeatureType>
        flowerSupport =
    {
        EnvironmentFeatureType::Grass,
        EnvironmentFeatureType::Shrub
    };


    updateFeatureType(
        EnvironmentFeatureType::Flower,
        flowerProfile,
        requestedFlowerCount,
        1.4f,
        2.3f,
        flowerSupport,
        18.0f,
        1
    );


    std::cout
        << "[VEGETATION]"
        << " trees="
        << countType(EnvironmentFeatureType::Tree)
        << "/" << spawnManager->getCapacity(treeProfile)

        << " grass="
        << countType(EnvironmentFeatureType::Grass)
        << "/" << spawnManager->getCapacity(grassProfile)

        << " shrubs="
        << countType(EnvironmentFeatureType::Shrub)
        << "/" << spawnManager->getCapacity(shrubProfile)

        << " flowers="
        << countType(EnvironmentFeatureType::Flower)
        << "/" << spawnManager->getCapacity(flowerProfile)

        << std::endl;
}


void VegetationManager::updateFeatureType(
    EnvironmentFeatureType type,
    const SpawnProfile& profile,
    int requestedCount,
    float minSize,
    float maxSize,
    const std::vector<EnvironmentFeatureType>&
        requiredNearbyTypes,
    float nearbyRadius,
    int minimumNearbyCount)
{
    // ------------------------------------------------------------
    // Remove features whose underlying habitat is no longer valid.
    // ------------------------------------------------------------

    features.erase(
        std::remove_if(
            features.begin(),
            features.end(),
            [this, type, &profile](
                const EnvironmentFeature& feature)
            {
                if (feature.type != type)
                    return false;

                return !spawnManager->isLocationSuitable(
                    profile,
                    feature.position
                );
            }
        ),
        features.end()
    );


    int capacity =
        spawnManager->getCapacity(profile);

    int target =
        std::min(
            requestedCount,
            capacity
        );


    // Reduce population if terrain shrinks.
    while (countType(type) > target)
    {
        removeLastOfType(type);
    }


    // Increase population when habitat becomes available.
    while (countType(type) < target)
    {
        if (!addFeature(
                type,
                profile,
                minSize,
                maxSize,
                requiredNearbyTypes,
                nearbyRadius,
                minimumNearbyCount))
        {
            break;
        }
    }


    // Refresh cached terrain information.
    for (auto& feature : features)
    {
        if (feature.type != type)
            continue;

        const HabitatCell* cell =
            habitatMap->getCellAt(
                feature.position
            );

        if (!cell)
            continue;

        feature.elevation =
            cell->elevation;

        feature.regionId =
            cell->regionId;
    }
}


bool VegetationManager::addFeature(
    EnvironmentFeatureType type,
    const SpawnProfile& profile,
    float minSize,
    float maxSize,
    const std::vector<EnvironmentFeatureType>&
        requiredNearbyTypes,
    float nearbyRadius,
    int minimumNearbyCount)
{
    std::vector<ofVec2f> existing =
        positionsOfType(type);


    // Several tries allow SpawnManager region balancing and our
    // local-spacing rule to work together.
    for (int attempt = 0;
         attempt < 80;
         ++attempt)
    {
        ofVec2f candidate;

        if (!spawnManager->findBalancedSpawnLocation(
                profile,
                existing,
                candidate))
        {
            return false;
        }


        if (!spacingIsValid(
                type,
                candidate,
                profile.minSpacing))
        {
            continue;
        }


        // Optional generic ecological dependency.
        //
        // Examples:
        // flowers near grass
        // mushrooms near trees
        // nests near shrubs
        // bones near future habitat features
        if (!requiredNearbyTypes.empty() &&
            minimumNearbyCount > 0)
        {
            if (!hasAnyFeatureNear(
                    requiredNearbyTypes,
                    candidate,
                    nearbyRadius,
                    minimumNearbyCount))
            {
                continue;
            }
        }


        const HabitatCell* cell =
            habitatMap->getCellAt(candidate);

        if (!cell)
            continue;


        EnvironmentFeature feature;

        feature.type =
            type;

        feature.position =
            candidate;

        feature.elevation =
            cell->elevation;

        feature.regionId =
            cell->regionId;

        feature.size =
            ofRandom(
                minSize,
                maxSize
            );


        features.push_back(feature);

        return true;
    }

    return false;
}


bool VegetationManager::spacingIsValid(
    EnvironmentFeatureType type,
    const ofVec2f& candidate,
    float minimumSpacing) const
{
    if (minimumSpacing <= 0.0f)
        return true;

    float minDistSq =
        minimumSpacing *
        minimumSpacing;


    for (const auto& feature : features)
    {
        if (feature.type != type)
            continue;

        if ((feature.position - candidate)
                .lengthSquared() <
            minDistSq)
        {
            return false;
        }
    }

    return true;
}


int VegetationManager::countType(
    EnvironmentFeatureType type) const
{
    int count = 0;

    for (const auto& feature : features)
    {
        if (feature.type == type)
            count++;
    }

    return count;
}


void VegetationManager::removeLastOfType(
    EnvironmentFeatureType type)
{
    for (auto it = features.rbegin();
         it != features.rend();
         ++it)
    {
        if (it->type == type)
        {
            features.erase(
                std::next(it).base()
            );

            return;
        }
    }
}


std::vector<ofVec2f>
VegetationManager::positionsOfType(
    EnvironmentFeatureType type) const
{
    std::vector<ofVec2f> result;

    result.reserve(features.size());

    for (const auto& feature : features)
    {
        if (feature.type == type)
        {
            result.push_back(
                feature.position
            );
        }
    }

    return result;
}


bool VegetationManager::hasFeatureNear(
    EnvironmentFeatureType type,
    const ofVec2f& position,
    float radius) const
{
    return countFeaturesNear(
        type,
        position,
        radius
    ) > 0;
}


int VegetationManager::countFeaturesNear(
    EnvironmentFeatureType type,
    const ofVec2f& position,
    float radius) const
{
    int count = 0;

    float radiusSq =
        radius * radius;


    for (const auto& feature : features)
    {
        if (feature.type != type)
            continue;

        if ((feature.position - position)
                .lengthSquared() <=
            radiusSq)
        {
            count++;
        }
    }

    return count;
}



bool VegetationManager::hasAnyFeatureNear(
    const std::vector<EnvironmentFeatureType>& types,
    const ofVec2f& position,
    float radius,
    int minimumFeatureCount) const
{
    if (minimumFeatureCount <= 0)
        return true;

    int count = 0;

    const float radiusSq =
        radius * radius;

    for (const auto& feature : features)
    {
        bool matchingType = false;

        for (const auto& type : types)
        {
            if (feature.type == type)
            {
                matchingType = true;
                break;
            }
        }

        if (!matchingType)
            continue;


        if ((feature.position - position)
                .lengthSquared() <= radiusSq)
        {
            count++;

            if (count >= minimumFeatureCount)
                return true;
        }
    }

    return false;
}


int VegetationManager::countFeaturesOfTypes(
    const std::vector<EnvironmentFeatureType>& types) const
{
    int count = 0;

    for (const auto& feature : features)
    {
        for (const auto& type : types)
        {
            if (feature.type == type)
            {
                count++;
                break;
            }
        }
    }

    return count;
}



bool VegetationManager::findNearestFeature(
    const std::vector<EnvironmentFeatureType>& types,
    const ofVec2f& position,
    float maxRadius,
    ofVec2f& featurePosition,
    float& distance) const
{
    const float maxRadiusSq =
        maxRadius * maxRadius;

    float bestDistanceSq =
        maxRadiusSq;

    bool found = false;


    for (const auto& feature : features)
    {
        bool matchingType = false;

        for (const auto& type : types)
        {
            if (feature.type == type)
            {
                matchingType = true;
                break;
            }
        }

        if (!matchingType)
            continue;


        float distanceSq =
            (feature.position - position)
                .lengthSquared();


        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq =
                distanceSq;

            featurePosition =
                feature.position;

            found = true;
        }
    }


    if (found)
    {
        distance =
            std::sqrt(bestDistanceSq);
    }

    return found;
}


int VegetationManager::getFeatureSupportCapacity(
    const std::vector<EnvironmentFeatureType>& types,
    int featuresPerObject) const
{
    if (featuresPerObject <= 0)
        return 0;

    return
        countFeaturesOfTypes(types) /
        featuresPerObject;
}


bool VegetationManager::findSpawnLocationNearFeatures(
    const SpawnProfile& terrainProfile,
    const std::vector<EnvironmentFeatureType>& requiredFeatures,
    float featureRadius,
    int minimumFeatureCount,
    const std::vector<ofVec2f>& existingPositions,
    ofVec2f& location) const
{
    if (!spawnManager ||
        !habitatMap ||
        !habitatMap->isReady())
    {
        return false;
    }


    // SpawnManager still controls valid terrain and region balance.
    // This adds a second requirement: useful environmental
    // features must also exist near the candidate position.
    for (int attempt = 0;
         attempt < 120;
         ++attempt)
    {
        ofVec2f candidate;

        if (!spawnManager->findBalancedSpawnLocation(
                terrainProfile,
                existingPositions,
                candidate))
        {
            return false;
        }


        if (!hasAnyFeatureNear(
                requiredFeatures,
                candidate,
                featureRadius,
                minimumFeatureCount))
        {
            continue;
        }


        location = candidate;
        return true;
    }

    return false;
}


// PINE_TREES_V2: cached overhead-oblique pines pointing uphill.
void VegetationManager::draw(
    const std::shared_ptr<KinectProjector>& projector) const
{
    if(!projector)return;
    ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);
    if(pineLoaded) {
        float now=ofGetElapsedTimef();
        if(fireGeneration!=MagicSandTreeFire::contactMap().generation) {
            fireGeneration=MagicSandTreeFire::contactMap().generation;
            for(auto& direction:pineDirections)direction.burn=MagicSandTreeFire::Burn();
            burningPines.clear();treeFireMesh.clear();nextPineMesh=0;nextTreeFireMesh=0;
        }
        if(now>=nextPineMesh) {nextPineMesh=now+.5f;rebuildPineMesh(projector);}
        pineImage.getTexture().bind();pineMesh.draw();pineImage.getTexture().unbind();
        if(now>=nextTreeFireMesh) {nextTreeFireMesh=now+.125f;rebuildTreeFireMesh(now);}
        treeFireMesh.draw();
    } else {
        for(const auto& feature:features)
            if(feature.type==EnvironmentFeatureType::Tree)drawTree(feature,projector);
    }
    // Retain grass/shrub/flower habitat records for animal ecology, while
    // hiding the old decorative circles and flower symbols. Terrain grass
    // textures are rendered by the existing terrain shader as before.
    ofPopStyle();
}

void VegetationManager::rebuildPineMesh(
    const std::shared_ptr<KinectProjector>& projector) const
{
    pineMesh.clear();pineMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    struct Tree {ofVec2f foot,up;float size,width;ofFloatColor color;};
    const float now=ofGetElapsedTimef();
    burningPines.clear();
    std::vector<Tree> trees;
    std::vector<PineDirection> directions;
    for(const auto& feature:features) {
        if(feature.type!=EnvironmentFeatureType::Tree)continue;
        const auto& pos=feature.position;
        auto foot=projector->kinectCoordToProjCoord(pos.x,pos.y);
        auto edge=projector->kinectCoordToProjCoord(pos.x+feature.size,pos.y);
        if(!std::isfinite(foot.x)||!std::isfinite(foot.y)||!std::isfinite(edge.x)||!std::isfinite(edge.y))continue;
        float radius=ofClamp((edge-foot).length(),4.0f,22.0f);
        MagicSandTreeFire::Burn burn;
        float oldAngle=-1.570796327f;bool previous=false;
        for(const auto& d:pineDirections) {
            auto delta=d.position-pos;
            if(delta.x*delta.x+delta.y*delta.y<.01f){oldAngle=d.angle;burn=d.burn;previous=true;
                // V78: avoid reprojecting stationary trees by sub-pixel amounts
                // on every cached-mesh refresh. A real terrain shift still moves
                // them at the same refresh rate, without interpolation or lag.
                if((foot-d.foot).length()<=1.5f)foot=d.foot;
                if(std::abs(radius-d.radius)<=.35f)radius=d.radius;
                break;}
        }
        float angle=oldAngle;
        if(habitatMap) {
            const float step=std::max(1.0f,float(habitatMap->getSampleStep()));
            const float probe=std::max(step*2.0f,feature.size*2.5f);
            // Weight nearby depth cells: center 4, cardinal neighbors 2,
            // diagonal neighbors 1. Use the stabilized habitat map, never
            // another Kinect readback. Reject incomplete edge neighborhoods.
            auto weightedHeight=[&](const ofVec2f& point,float& height) {
                float sum=0,weightSum=0;
                for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
                    const auto* cell=habitatMap->getCellAt(point+ofVec2f(x*step,y*step));
                    if(!cell||!cell->valid||!std::isfinite(cell->physicalElevation))continue;
                    float weight=float((x==0?2:1)*(y==0?2:1));
                    sum+=cell->physicalElevation*weight;weightSum+=weight;
                }
                if(weightSum<12.0f)return false;
                height=sum/weightSum;return true;
            };
            float baseHeight=0;
            if(weightedHeight(pos,baseHeight)) {
                float bestHeight=baseHeight+.75f;
                float bestAngle=oldAngle;
                float retainedHeight=-std::numeric_limits<float>::infinity();
                float retainedAngle=oldAngle,closestAngle=10.0f;
                // Search the whole neighborhood, rather than averaging
                // opposing slopes into a direction between two hills.
                for(int n=0;n<16;++n) {
                    float bearing=n*6.283185307f/16;
                    ofVec2f target=pos+ofVec2f(std::cos(bearing),std::sin(bearing))*probe;
                    float height=0;
                    if(!weightedHeight(target,height))continue;
                    auto projected=projector->kinectCoordToProjCoord(target.x,target.y)-foot;
                    if(!std::isfinite(projected.x)||!std::isfinite(projected.y)||projected.length()<.1f)continue;
                    float desired=std::atan2(projected.y,projected.x);
                    if(height>bestHeight){bestHeight=height;bestAngle=desired;}
                    float turn=std::abs(std::atan2(std::sin(desired-oldAngle),std::cos(desired-oldAngle)));
                    if(turn<closestAngle){closestAngle=turn;retainedHeight=height;retainedAngle=desired;}
                }
                if(bestHeight>baseHeight+.75f) {
                    angle=bestAngle;
                    // Keep an almost-equally-high direction to avoid jitter,
                    // but only while it is still uphill. A changed slope
                    // turns directly uphill, never slowly through downhill.
                    if(previous&&retainedHeight>baseHeight+.75f&&bestHeight-retainedHeight<.5f)
                        angle=retainedAngle;
                }
                // Flat ground or a local summit has no higher neighbor:
                // retain the last heading instead of inventing an uphill side.
            }
        }
        burn.update(MagicSandTreeFire::contactMap().touches(pos.x,pos.y,
            std::max(2.0f,feature.size*.7f),now),now);
        directions.push_back({pos,angle,burn,foot,radius});
        float seed=std::sin(pos.x*12.9898f+pos.y*78.233f)*43758.5453f;
        seed-=std::floor(seed);
        float height=ofClamp(radius*3.0f,30.0f,68.0f)*(.92f+.16f*seed);
        float width=height*(.94f+.10f*seed);
        const float charred=burn.charAmount(now);
        if(burn.burning(now))burningPines.push_back({foot,
            ofVec2f(std::cos(angle),std::sin(angle)),height,seed,burn.started});
        trees.push_back({foot,ofVec2f(std::cos(angle),std::sin(angle)),height,width,
            ofFloatColor((.92f+.08f*seed)*(1-.86f*charred),
                         (.96f+.04f*seed)*(1-.90f*charred),
                         (.91f+.07f*seed)*(1-.91f*charred),1)});
    }
    pineDirections.swap(directions);
    std::sort(trees.begin(),trees.end(),[](const Tree& a,const Tree& b){return a.foot.y<b.foot.y;});
    const int triangles[]={0,1,2,0,2,3};
    const ofVec2f fraction[]={ofVec2f(0,0),ofVec2f(1,0),ofVec2f(1,1),ofVec2f(0,1)};
    ofVec2f uv[4];for(int i=0;i<4;++i)uv[i]=pineImage.getTexture().getCoordFromPercent(fraction[i].x,fraction[i].y);
    for(const auto& tree:trees) {
        ofVec2f right(-tree.up.y,tree.up.x);
        // The generated trunk base is at (49%,83%) of its transparent canvas.
        for(int n:triangles) {
            float horizontal=(fraction[n].x-.49f)*tree.width;
            float vertical=(.83f-fraction[n].y)*tree.size;
            auto p=tree.foot+right*horizontal+tree.up*vertical;
            pineMesh.addVertex(ofVec3f(p.x,p.y,0));pineMesh.addColor(tree.color);pineMesh.addTexCoord(uv[n]);
        }
    }
}


// One small, untextured VBO for all fire and smoke; rebuilt at 8 Hz.
// No per-frame particles or extra terrain/camera sampling.
void VegetationManager::rebuildTreeFireMesh(float now) const
{
    if(burningPines.empty()&&treeFireMesh.getNumVertices()==0)return;
    treeFireMesh.clear();treeFireMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    auto vertex=[&](const ofVec2f& p,const ofFloatColor& color) {
        treeFireMesh.addVertex(ofVec3f(p.x,p.y,0));treeFireMesh.addColor(color);
    };
    for(const auto& tree:burningPines) {
        float age=now-tree.started;
        if(age<0||age>=14)continue;
        float strength=std::min(1.0f,age*2.0f)*std::min(1.0f,(14-age)/3);
        ofVec2f side(-tree.up.y,tree.up.x);
        // Semi-transparent smoke fans drift away from the burning canopy.
        for(int n=0;n<3;++n) {
            float phase=std::fmod(age*.3f+n*.333f+tree.seed,1.0f);
            auto center=tree.foot+tree.up*(tree.size*(.38f+phase*.35f))
                +side*(tree.size*phase*.3f);
            float radius=tree.size*(.07f+phase*.13f);
            for(int j=0;j<8;++j) {
                float a=j*6.283185307f/8,b=(j+1)*6.283185307f/8;
                vertex(center,ofFloatColor(.20f,.18f,.16f,strength*(1-phase)*.20f));
                vertex(center+ofVec2f(std::cos(a),std::sin(a))*radius,ofFloatColor(.12f,.11f,.10f,0));
                vertex(center+ofVec2f(std::cos(b),std::sin(b))*radius,ofFloatColor(.12f,.11f,.10f,0));
            }
        }
        // Three flame clusters spread from the trunk into the crown.
        for(int cluster=0;cluster<3;++cluster) {
            float spread=std::min(1.0f,age/2.5f);
            auto base=tree.foot+tree.up*(tree.size*(.10f+cluster*.20f*spread));
            for(int layer=0;layer<2;++layer) {
                float pulse=.80f+.20f*std::sin(now*11+cluster*2+tree.seed*8);
                float width=tree.size*(layer==0?.11f:.055f)*strength;
                float height=tree.size*(layer==0?.29f:.18f)*pulse*strength;
                ofVec2f rise=tree.up*.8f+ofVec2f(0,-.2f);
                auto tip=base+rise*height+side*(width*.35f*std::sin(now*9+cluster));
                ofVec2f outline[]={base-side*width,base-side*width*.65f+rise*height*.45f,
                    tip,base+side*width*.6f+rise*height*.4f,base+side*width};
                auto center=base+rise*height*.28f;
                ofFloatColor bright=layer==0?ofFloatColor(1,.48f,.025f,.92f*strength):ofFloatColor(1,.95f,.40f,.95f*strength);
                for(int j=0;j<5;++j) {
                    vertex(center,bright);
                    vertex(outline[j],ofFloatColor(1,layer==0?.12f:.65f,0,0));
                    vertex(outline[(j+1)%5],ofFloatColor(1,layer==0?.12f:.65f,0,0));
                }
            }
        }
    }
}


void VegetationManager::drawTree(
    const EnvironmentFeature& feature,
    const std::shared_ptr<KinectProjector>& projector) const
{
    ofVec2f p =
        projector->kinectCoordToProjCoord(
            feature.position.x,
            feature.position.y
        );

    ofVec2f edge =
        projector->kinectCoordToProjCoord(
            feature.position.x +
                feature.size,
            feature.position.y
        );

    float radius =
        ofClamp(
            (edge - p).length(),
            4.0f,
            22.0f
        );


    ofSetColor(35, 92, 40, 235);
    ofDrawCircle(
        p,
        radius
    );

    ofSetColor(47, 115, 52, 235);

    ofDrawCircle(
        p.x - radius * 0.35f,
        p.y - radius * 0.15f,
        radius * 0.62f
    );

    ofDrawCircle(
        p.x + radius * 0.32f,
        p.y - radius * 0.18f,
        radius * 0.58f
    );

    ofDrawCircle(
        p.x,
        p.y + radius * 0.28f,
        radius * 0.60f
    );

    ofSetColor(95, 67, 38, 245);

    ofDrawCircle(
        p,
        std::max(
            1.5f,
            radius * 0.16f
        )
    );
}


void VegetationManager::drawGrass(
    const EnvironmentFeature& feature,
    const std::shared_ptr<KinectProjector>& projector) const
{
    ofVec2f p =
        projector->kinectCoordToProjCoord(
            feature.position.x,
            feature.position.y
        );

    ofVec2f edge =
        projector->kinectCoordToProjCoord(
            feature.position.x +
                feature.size,
            feature.position.y
        );

    float radius =
        ofClamp(
            (edge - p).length(),
            2.5f,
            12.0f
        );


    // Soft patch rather than individual blades.
    // Extremely cheap to draw and easy to identify visually.
    ofSetColor(70, 130, 52, 135);

    ofDrawCircle(
        p,
        radius
    );

    ofSetColor(90, 155, 62, 125);

    ofDrawCircle(
        p.x - radius * 0.35f,
        p.y + radius * 0.10f,
        radius * 0.55f
    );

    ofDrawCircle(
        p.x + radius * 0.30f,
        p.y - radius * 0.15f,
        radius * 0.50f
    );
}


void VegetationManager::drawShrub(
    const EnvironmentFeature& feature,
    const std::shared_ptr<KinectProjector>& projector) const
{
    ofVec2f p =
        projector->kinectCoordToProjCoord(
            feature.position.x,
            feature.position.y
        );

    ofVec2f edge =
        projector->kinectCoordToProjCoord(
            feature.position.x +
                feature.size,
            feature.position.y
        );

    float radius =
        ofClamp(
            (edge - p).length(),
            3.0f,
            16.0f
        );


    ofSetColor(18, 66, 28, 240);

    ofDrawCircle(
        p.x - radius * 0.30f,
        p.y,
        radius * 0.66f
    );

    ofDrawCircle(
        p.x + radius * 0.30f,
        p.y,
        radius * 0.66f
    );

    ofSetColor(28, 88, 36, 240);

    ofDrawCircle(
        p.x,
        p.y - radius * 0.22f,
        radius * 0.65f
    );
}


void VegetationManager::drawFlower(
    const EnvironmentFeature& feature,
    const std::shared_ptr<KinectProjector>& projector) const
{
    ofVec2f p =
        projector->kinectCoordToProjCoord(
            feature.position.x,
            feature.position.y
        );


    ofVec2f edge =
        projector->kinectCoordToProjCoord(
            feature.position.x +
                feature.size,
            feature.position.y
        );


    float radius =
        ofClamp(
            (edge - p).length(),
            2.0f,
            8.0f
        );


    // Deterministic color selection based on position.
    // This prevents colors from flickering between frames.
    int colorIndex =
        (
            static_cast<int>(feature.position.x) +
            static_cast<int>(feature.position.y)
        ) % 4;


    ofColor petalColor;

    if (colorIndex == 0)
        petalColor = ofColor(240, 185, 210, 245);
    else if (colorIndex == 1)
        petalColor = ofColor(215, 190, 245, 245);
    else if (colorIndex == 2)
        petalColor = ofColor(245, 225, 160, 245);
    else
        petalColor = ofColor(225, 235, 245, 245);


    float petalRadius =
        radius * 0.42f;

    float petalOffset =
        radius * 0.47f;


    ofSetColor(petalColor);


    // Four extremely cheap petals.
    ofDrawCircle(
        p.x - petalOffset,
        p.y,
        petalRadius
    );

    ofDrawCircle(
        p.x + petalOffset,
        p.y,
        petalRadius
    );

    ofDrawCircle(
        p.x,
        p.y - petalOffset,
        petalRadius
    );

    ofDrawCircle(
        p.x,
        p.y + petalOffset,
        petalRadius
    );


    // Bright center.
    ofSetColor(245, 190, 55, 255);

    ofDrawCircle(
        p,
        std::max(
            1.2f,
            radius * 0.28f
        )
    );
}
