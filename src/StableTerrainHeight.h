#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
// STEADY_TERRAIN_V78: display/habitat hysteresis, never a camera filter.
// Hold millimetre-scale jitter, confirm small sustained changes, and accept
// larger edits immediately at the existing update cadence. No quantization.
struct StableTerrainHeight {
    float value=0,candidate=0,since=-1;
    bool valid=false;
    float update(float measured,float now){
        if(!std::isfinite(measured)||!std::isfinite(now))return value;
        if(!valid){value=measured;valid=true;since=-1;return value;}
        float difference=std::abs(measured-value);
        if(difference<=2.f){since=-1;return value;}
        if(difference>=6.f){value=measured;since=-1;return value;}
        if(since<0||now<since||std::abs(measured-candidate)>1.5f){candidate=measured;since=now;return value;}
        candidate=(candidate+measured)*.5f;
        if(now-since>=.24f){value=candidate;since=-1;}
        return value;
    }
};
