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
#include <array>
#include <cmath>
// FOSSIL_ROI_V33: follow geometry changes, but never live digging/depth changes.
class FossilBoxGeometry {
public:
    void reset(){ready=false;}
    template<class Projector> bool changed(Projector& projector){
        std::array<float,40> current{};int n=0;
        auto roi=projector.getKinectROI();
        current[n++]=roi.x;current[n++]=roi.y;current[n++]=roi.width;current[n++]=roi.height;
        auto plane=projector.getBasePlaneEq();
        current[n++]=plane.x;current[n++]=plane.y;current[n++]=plane.z;current[n++]=plane.w;
        auto camera=projector.getTransposedKinectWorldMatrix();
        auto projection=projector.getTransposedKinectProjMatrix();
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)current[n++]=camera(row,col);
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)current[n++]=projection(row,col);
        const bool different=ready&&current!=key;key=current;ready=true;return different;
    }
    static ofVec2f fit(const ofRectangle& roi,float aspect,float degrees){
        if(!std::isfinite(aspect)||aspect<=0||roi.width<=0||roi.height<=0)return {0,0};
        const float maxW=roi.width*.92f,maxH=roi.height*.92f;
        float w=maxW,h=w/aspect;
        if(h>maxH){h=maxH;w=h*aspect;}
        const float radians=degrees*.017453292519943295f;
        const float c=std::abs(std::cos(radians)),s=std::abs(std::sin(radians));
        const float rotatedW=c*w+s*h,rotatedH=s*w+c*h;
        const float scale=std::min(1.f,std::min(maxW/std::max(.001f,rotatedW),maxH/std::max(.001f,rotatedH)));
        return {w*scale,h*scale};
    }
private:
    bool ready=false;std::array<float,40> key{};
};
