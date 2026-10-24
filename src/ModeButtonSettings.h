#pragma once
#include "ofMain.h"
// MODE_BUTTONS_V37: operator lockouts, separate from whether a mode is active.
namespace MagicSandModeButtons {
struct Settings{bool fossil=true,dig=false;};
inline Settings& values(){static Settings settings;return settings;}
inline bool shown(int slot){return slot==0?values().fossil:slot==1?values().dig:false;}
inline void capture(ofJson& j){j["projected_fossil_button"]=values().fossil;j["projected_dig_button"]=values().dig;}
inline void restore(const ofJson& j){values().fossil=j.value("projected_fossil_button",true);values().dig=j.value("projected_dig_button",false);}
}
