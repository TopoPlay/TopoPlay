/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ofMain.h"
#include "KinectProjector/KinectProjector.h"
#include "TriggerSettings.h"
// Small cached mesh masks projection on objects above the landscape ceiling.
// Uses existing CPU depth snapshots; no GPU readback or new camera stream.
class CeilingMask {
public:
 void update(const std::shared_ptr<KinectProjector>& k){
    const float now=ofGetElapsedTimef();
    if(!k||k->GetApplicationState()!=KinectProjector::APPLICATION_STATE_RUNNING){mesh.clear();return;}
    if(now<next)return;next=now+.12f;uint64_t s=0;
    if(!k->copyRainDepthSnapshot(raw,s)||!raw.isAllocated()) {mesh.clear();return;}
    if(s==serial){if(now-fresh>.45f)mesh.clear();return;}serial=s;fresh=now;
    mesh.clear();mesh.setMode(OF_PRIMITIVE_TRIANGLES);const auto roi=k->getKinectROI();
    const int step=5;
    for(int y=int(roi.y);y<int(roi.y+roi.height);y+=step)for(int x=int(roi.x);x<int(roi.x+roi.width);x+=step){
        if(x<0||y<0||x>=int(raw.getWidth())||y>=int(raw.getHeight()))continue;
        float d=raw[y*raw.getWidth()+x];if(d<450||d>4500)continue;
        if(!MagicSandTriggers::handHeight(k->rainElevationFromDepth(x,y,d)))continue;
        ofVec2f v[4];bool valid=true;const float dx[]={-3,3,3,-3},dy[]={-3,-3,3,3};
        for(int n=0;n<4;++n){v[n]=k->kinectCoordToProjCoord(x+dx[n],y+dy[n],d);valid=valid&&std::isfinite(v[n].x)&&std::isfinite(v[n].y);}
        if(valid){for(int n:{0,1,2,0,2,3})mesh.addVertex(ofVec3f(v[n].x,v[n].y,0));}
    }
 }
 void draw(){if(mesh.getNumVertices()==0)return;
    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
    ofPushStyle();ofSetColor(0,0,0,255);mesh.draw();ofPopStyle();
    if(depth)glEnable(GL_DEPTH_TEST);if(cull)glEnable(GL_CULL_FACE);if(stencil)glEnable(GL_STENCIL_TEST);
 }
private:ofVboMesh mesh;ofShortPixels raw;uint64_t serial=0;float fresh=-1000,next=0;
};
