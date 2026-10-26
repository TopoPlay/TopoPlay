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
#include "ofMain.h"
// MODE_BUTTONS_V37: operator lockouts, separate from whether a mode is active.
namespace MagicSandModeButtons {
struct Settings{bool fossil=true,dig=false;};
inline Settings& values(){static Settings settings;return settings;}
inline bool shown(int slot){return slot==0?values().fossil:slot==1?values().dig:false;}
inline void capture(ofJson& j){j["projected_fossil_button"]=values().fossil;j["projected_dig_button"]=values().dig;}
inline void restore(const ofJson& j){values().fossil=j.value("projected_fossil_button",true);values().dig=j.value("projected_dig_button",false);}
}
