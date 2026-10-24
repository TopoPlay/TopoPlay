/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ofMain.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Shared calibrated-waterline settings for physical gestures. Accessed only
// from the application thread; no camera readbacks or privileged work.
namespace MagicSandTriggers {
struct Values {
    float volcanoPeak=0;       // 0 keeps the existing mound-shape trigger
    float handFloor=280;       // mm ABOVE the calibrated water level
    float ringScale=1.5f;      // radius multiplier for raised finger rings
    float buttonHold=1.0f;     // seconds, independent of application FPS
};
inline Values& values(){static Values result;return result;}
inline unsigned& revision(){static unsigned result=0;return result;}
inline float& observedPeak(){static float result=std::numeric_limits<float>::quiet_NaN();return result;}
inline float bounded(float value,float fallback,float lo,float hi) {
    return std::isfinite(value)?std::max(lo,std::min(hi,value)):fallback;
}
inline bool normalize(Values& v) {
    v.volcanoPeak=bounded(v.volcanoPeak,0,0,500);
    v.handFloor=bounded(v.handFloor,280,50,600);
    v.ringScale=bounded(v.ringScale,1.5f,.8f,2.5f);
    v.buttonHold=bounded(v.buttonHold,1,.25f,3);
    bool raised=v.volcanoPeak>0&&v.volcanoPeak>v.handFloor-10;
    if(raised)v.volcanoPeak=std::max(0.0f,v.handFloor-10); // CONTROLS_V3: ceiling owns the hand boundary
    return raised;
}
inline bool apply(Values v) {
    bool raised=normalize(v);auto& old=values();
    if(v.ringScale!=old.ringScale||v.volcanoPeak!=old.volcanoPeak||v.handFloor!=old.handFloor||v.buttonHold!=old.buttonHold) {
        old=v;++revision();
    }
    return raised;
}
inline void save(){try{ofJson j;auto v=values();j["volcano_peak_mm"]=v.volcanoPeak;j["hand_floor_mm"]=v.handFloor;j["button_hold_seconds"]=v.buttonHold;j["button_ring_scale"]=v.ringScale;ofSavePrettyJson(ofToDataPath("trigger-settings.json",true),j);}catch(const std::exception&) {}}
inline void setCeiling(float mm){auto v=values();v.handFloor=mm;apply(v);save();}
inline bool handHeight(float height) {
    return std::isfinite(height)&&height>values().handFloor;
}
inline bool peakHeight(float height) {
    return std::isfinite(height)&&(values().volcanoPeak<=0||height>=values().volcanoPeak);
}
}
