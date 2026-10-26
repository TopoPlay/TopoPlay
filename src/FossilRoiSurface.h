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
#include "KinectProjector/KinectProjector.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
// FOSSIL_SURFACE_V35: the fossil sand background follows the same live camera
// ROI and native projection as the landscape, rather than the full projector.
// A full-projector rectangle paints beyond the calibrated sandbox footprint.
// Build the background from camera ROI samples instead, and refresh at 10 Hz
// to follow edits while avoiding a full mesh rebuild on every display frame.
class FossilRoiSurface {
public:
    void draw(const std::shared_ptr<KinectProjector>& projector,ofImage* image){
        if(!projector)return;
        const auto current=projector->getKinectROI();
        if(current.width<2||current.height<2)return;
        const float now=ofGetElapsedTimef();
        if(current!=roi||projector->isCalibrationUpdated()||projector->isBasePlaneUpdated()||now>=nextBuild){
            const bool resized=current!=roi;roi=current;nextBuild=now+.10f;build(*projector,image);
            if(resized)std::cout<<"[FOSSIL SURFACE V35] roi="<<roi.x<<","<<roi.y<<","<<roi.width<<","<<roi.height
                <<"; background uses calibrated ROI, not full projector"<<std::endl;
        }
        ofPushStyle();ofEnableAlphaBlending();
        if(image){ofSetColor(255);image->getTexture().bind();mesh.draw();image->getTexture().unbind();}
        else{ofSetColor(205,180,120,255);mesh.draw();}
        ofPopStyle();
    }
private:
    ofRectangle roi;float nextBuild=-1000;ofVboMesh mesh;
    void build(KinectProjector& projector,ofImage* image){
        mesh.clear();mesh.setMode(OF_PRIMITIVE_TRIANGLES);
        // ~4 camera pixels per cell, bounded at 100 cells per axis. No normal
        // elevation renderer, camera acquisition, particles or extra framebuffer.
        const int nx=std::max(1,std::min(100,int(std::ceil(roi.width/4.f))));
        const int ny=std::max(1,std::min(100,int(std::ceil(roi.height/4.f))));
        struct Point{ofVec3f projected;ofVec2f uv;bool valid=false;};
        std::vector<Point> points((nx+1)*(ny+1));
        for(int y=0;y<=ny;++y)for(int x=0;x<=nx;++x){
            const float u=float(x)/nx,v=float(y)/ny;
            const float kx=roi.x+u*(roi.width-1),ky=roi.y+v*(roi.height-1);
            auto& p=points[y*(nx+1)+x];
            const auto world=projector.kinectCoordToWorldCoord(kx,ky);
            if(!std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z)||world.z<450||world.z>4500)continue;
            const auto screen=projector.worldCoordToProjCoord(world);
            if(!std::isfinite(screen.x)||!std::isfinite(screen.y))continue;
            p.projected=ofVec3f(screen.x,screen.y,0);p.valid=true;
            if(image)p.uv=image->getTexture().getCoordFromPercent(u,v);
        }
        const int triangles[]={0,1,2,0,2,3};
        for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
            int ids[]={y*(nx+1)+x,y*(nx+1)+x+1,(y+1)*(nx+1)+x+1,(y+1)*(nx+1)+x};
            if(!points[ids[0]].valid||!points[ids[1]].valid||!points[ids[2]].valid||!points[ids[3]].valid)continue;
            for(int n:triangles){const auto& p=points[ids[n]];mesh.addVertex(p.projected);if(image)mesh.addTexCoord(p.uv);}
        }
    }
};
