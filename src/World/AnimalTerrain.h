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
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <algorithm>
#include <cmath>

// Application-thread-only ecology view of the effective rendered boundaries.
// Camera/projector coordinates, geometric slopes, rain and lava retain physical
// millimeters. Mapping onto the original material scale lets existing deep-water
// buffers and plant habitat tests follow customized, shallow or shifted bands.
namespace MagicSandAnimalTerrain {
using Levels=std::array<float,20>;
inline Levels reference(){return {{-220,-180,-130,-70,-15,-4,-1,0,4,5,15,51,65,80,96,134,165,175,190,210}};}
struct Profile {Levels levels=reference();bool enabled=false;unsigned revision=0;};
inline Profile& profile(){static Profile value;return value;}
inline void publish(bool enabled,const Levels& levels){
    bool valid=true;
    for(int n=0;n<20;++n)if(!std::isfinite(levels[n]) || (n>0 && levels[n]<levels[n-1]))valid=false;
    enabled=enabled && valid;
    auto& p=profile();
    if(p.enabled==enabled && (!enabled || p.levels==levels))return;
    p.enabled=enabled;if(enabled)p.levels=levels;++p.revision;
}
inline float elevation(float physical){
    const auto& p=profile();
    if(!p.enabled || !std::isfinite(physical))return physical;
    const auto& levels=p.levels;
    if(physical>=levels[19])return physical-levels[19]+210.0f;
    if(physical<=levels[0])return physical-levels[0]-220.0f;
    int low=0,high=19;
    for(int step=0;step<5;++step){
        if(high-low>1){int mid=(low+high)/2;if(physical<levels[mid])high=mid;else low=mid;}
    }
    const auto ref=reference();
    float span=levels[high]-levels[low];
    float t=span>0?std::max(0.0f,std::min(1.0f,(physical-levels[low])/span)):1.0f;
    return ref[low]+t*(ref[high]-ref[low]);
}
}
