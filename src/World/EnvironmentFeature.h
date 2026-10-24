/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
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
