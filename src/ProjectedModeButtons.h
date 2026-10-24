/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ModeButtonSettings.h"
#include "AutoBonePlacement.h"
namespace MagicSandProjectedButtons {
// Wordless shovel fits within the existing bone footprint clearance check.
inline void drawDig(ofVec2f center,float rx,float ry,bool active,bool pressed){
    static ofPath shape;static bool ready=false;static int previous=-1;
    if(!ready){
        shape.moveTo(-.45f,-.70f);shape.lineTo(.45f,-.70f);shape.lineTo(.45f,-.50f);
        shape.lineTo(.13f,-.50f);shape.lineTo(.13f,.02f);shape.lineTo(.56f,.02f);
        shape.lineTo(.48f,.50f);shape.lineTo(0,.70f);shape.lineTo(-.48f,.50f);
        shape.lineTo(-.56f,.02f);shape.lineTo(-.13f,.02f);shape.lineTo(-.13f,-.50f);
        shape.lineTo(-.45f,-.50f);shape.close();shape.setFilled(true);ready=true;
    }
    const int state=(active?2:0)+(pressed?1:0);
    if(previous!=state){previous=state;shape.setFillColor(active?ofColor(40,205,85,255):ofColor(248,236,199,255));
        shape.setStrokeColor(pressed?ofColor(255,235,70,255):ofColor(40,35,25,255));shape.setStrokeWidth(pressed?.15f:.10f);}
    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
    ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);ofPushMatrix();ofTranslate(center.x,center.y);ofScale(rx*.28f,ry*.28f);
    shape.draw();ofPopMatrix();ofPopStyle();
    if(depth)glEnable(GL_DEPTH_TEST);if(cull)glEnable(GL_CULL_FACE);if(stencil)glEnable(GL_STENCIL_TEST);
}
}
