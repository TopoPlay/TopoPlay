/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
// SMALL_HANDS_V5: sample a dense radial band rather than eight sparse circles.
// A small center opening and thin fingers receive the same height gate.
namespace MagicSandRing {
template<class Sample> int sections(Sample sample,float cx,float cy,float radius,float centerRadius,float ceiling){
    // ONE_PIXEL_CENTER_V19: only the existing center depth pixel must be clear.
    // The surrounding ring search and its coverage rules remain unchanged.
    // The parameter remains for call-site compatibility. A wider center test
    // rejected small paper rings and children's hands, so only the center pixel
    // must remain below the gesture ceiling. Radius still bounds the outer search.
    (void)centerRadius;
    const float middle=sample(cx,cy);
    if(!std::isfinite(middle)||middle>ceiling)return 0;
    // Detect small hands independently of the GUI's outer ring-size setting.
    // One-pixel radial spacing avoids jumping over a thin finger between
    // sparse test circles. Slight angular offsets cover pixel-grid gaps.
    const float outer=std::max(5.0f,std::min(132.0f,radius*1.65f));
    int matched=0;bool quadrant[4]={false,false,false,false};
    for(int sector=0;sector<16;++sector){
        bool found=false;
        for(float jitter:{0.0f,-.075f,.075f}){
            const float angle=6.28318530718f*sector/16+jitter;
            const float x=std::cos(angle),y=std::sin(angle);
            for(float r=2.5f;r<=outer;r+=.75f){
                float h=sample(cx+x*r,cy+y*r);
                if(std::isfinite(h)&&h>ceiling&&h-middle>=4){found=true;break;}
            }
            if(found)break;
        }
        if(found){++matched;quadrant[sector/4]=true;}
    }
    for(bool covered:quadrant)if(!covered)return std::min(7,matched);
    return matched;
}
}
