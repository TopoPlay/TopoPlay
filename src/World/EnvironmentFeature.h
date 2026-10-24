#pragma once

#include "ofMain.h"

// Generic environmental features.
//
// Animals should eventually depend on these features rather than
// containing hard-coded knowledge of specific vegetation classes.
enum class EnvironmentFeatureType
{
    Tree,
    Grass,
    Shrub,
    Flower,
    Rock,
    Bone
};


struct EnvironmentFeature
{
    EnvironmentFeatureType type =
        EnvironmentFeatureType::Tree;

    // Kinect-space terrain location.
    ofVec2f position;

    // Cached terrain information.
    float elevation = 0.0f;
    int regionId = -1;

    // Visual/physical scale in approximate Kinect pixels.
    float size = 4.0f;
};
