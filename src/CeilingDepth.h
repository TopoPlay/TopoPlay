/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
// Calibrated height above water: h=(a*x+b*y+c)*depth+bias.
// Owned by the acquisition thread; updated through its action queue.
struct MagicSandCeilingDepth {
    bool enabled=false;float a=0,b=0,c=-1,bias=1000,height=280;
    bool accepts(int x,int y,float depth,float oldCutoff)const {
        if(!enabled)return depth>oldCutoff;
        const float h=(a*x+b*y+c)*depth+bias;
        return std::isfinite(depth)&&depth>=450&&depth<=4500&&std::isfinite(h)&&h<=height;
    }
};
