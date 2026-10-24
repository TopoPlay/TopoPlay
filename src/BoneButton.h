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
// BONE_BUTTON_V8: cached vector artwork; detection still uses the same center.
namespace MagicSandBone {
inline ofVec2f corner(const ofRectangle& roi,float radius,float ringScale){
    // BONE_CORNER_V9: fixed artwork inset, independent of the sensing slider.
    // Raw-depth gesture sampling may extend past the landscape ROI.
    const float wanted=std::max(18.0f,radius*.65f+4.0f);
    const float mx=std::min(wanted,roi.width*.45f);
    // BONE_SIDE_CENTER_V14: same projected side, halfway down its length.
    // Kinect X is mirrored relative to the projected sandbox.
    return {roi.x+roi.width-mx,roi.y+roi.height*.5f};
}
inline void shape(ofPath& p){
    p.setCurveResolution(12);p.moveTo(-.60f,-.18f);
    p.bezierTo(-.65f,-.62f,-1.28f,-.70f,-1.30f,-.26f);
    p.bezierTo(-1.31f,-.12f,-1.24f,-.03f,-1.12f,0);
    p.bezierTo(-1.24f,.03f,-1.31f,.12f,-1.30f,.26f);
    p.bezierTo(-1.28f,.70f,-.65f,.62f,-.60f,.18f);
    p.lineTo(.60f,.18f);
    p.bezierTo(.65f,.62f,1.28f,.70f,1.30f,.26f);
    p.bezierTo(1.31f,.12f,1.24f,.03f,1.12f,0);
    p.bezierTo(1.24f,-.03f,1.31f,-.12f,1.30f,-.26f);
    p.bezierTo(1.28f,-.70f,.65f,-.62f,.60f,-.18f);p.close();
}
inline void draw(ofVec2f center,float rx,float ry,bool active,bool pressed){
    static ofPath bone,edge;static bool ready=false;static int last=-1;
    if(!ready){shape(bone);shape(edge);bone.setFilled(true);edge.setFilled(false);edge.setStrokeColor(ofColor(15,24,20,245));edge.setStrokeWidth(.20f);ready=true;}
    const int state=(active?2:0)+(pressed?1:0);
    if(state!=last){last=state;
        bone.setFillColor(active?ofColor(40,205,85,255):ofColor(248,236,199,255));
        bone.setStrokeColor(pressed?ofColor(255,235,70,255):(active?ofColor(180,255,195,255):ofColor(92,69,43,255)));
        bone.setStrokeWidth(pressed?.15f:.075f);
    }
    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
    ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);ofPushMatrix();ofTranslate(center.x,center.y);ofScale(rx*.30f,ry*.30f);
    edge.draw();bone.draw();ofPopMatrix();ofPopStyle();
    if(depth)glEnable(GL_DEPTH_TEST);if(cull)glEnable(GL_CULL_FACE);if(stencil)glEnable(GL_STENCIL_TEST);
}
}
