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
#pragma once

#include <string>
#include <limits>

// Generic requirements for anything that can be placed in the sandbox.
//
// Examples:
//   fish, sharks, rabbits
//   trees, flowers, bones
//   future elevation-specific creatures or objects
//
// SpawnManager interprets these requirements against TerrainHabitatMap.
// No game-specific classes should be referenced here.
struct SpawnProfile
{
    std::string name;

    bool requireWater = false;
    bool requireLand = false;

    float minElevation = -std::numeric_limits<float>::max();
    float maxElevation =  std::numeric_limits<float>::max();

    float minSlope = 0.0f;
    float maxSlope = std::numeric_limits<float>::max();

    // Ignore connected habitat regions smaller than this.
    // Units will initially be approximate Kinect-pixel area.
    float minRegionArea = 0.0f;

    // Approximate amount of usable habitat required per object.
    // 0 means habitat area does not impose a population limit.
    float areaPerObject = 0.0f;

    // Minimum desired spacing between objects.
    // Initially used for spawning rather than movement.
    float minSpacing = 0.0f;
};
