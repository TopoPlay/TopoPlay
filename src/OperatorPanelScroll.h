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
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ofMain.h"
#include "OperatorLayout.h"
#include <algorithm>
// Fixed navigation outside the scrolling GUI rectangles. This strip belongs to
// the laptop operator window only; it never reaches the projector compositor.
namespace MagicSandPanelScroll {
inline MagicSandOperator::Rect bar(float width){
    float x=MagicSandOperator::columnWidth(width)+16;
    float available=std::max(1.0f,width-2*x);
    return {x,8,std::min(330.0f,available),32};
}
inline bool hit(float x,float y,const MagicSandOperator::Rect& r){return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h;}
inline bool handle(int x,int y,int button,float width,float height,float& offset,float maximum){
    auto r=bar(width);
    if(button!=0 || maximum<=0 || !hit(float(x),float(y),r))return false;
    float caption=std::min(70.0f,r.w*.3f),buttonWidth=(r.w-caption)/3;
    int which=int((x-r.x-caption)/buttonWidth);
    if(x<r.x+caption)return true;
    float page=std::max(80.0f,height*.6f);
    if(which==0)offset=0;
    else if(which==1)offset=std::max(0.0f,offset-page);
    else if(which==2)offset=std::min(maximum,offset+page);
    return true;
}
inline void draw(float width,float offset,float maximum){
    if(maximum<=0)return;
    auto r=bar(width);float caption=std::min(70.0f,r.w*.3f),buttonWidth=(r.w-caption)/3;
    ofPushStyle();ofFill();ofSetColor(30);ofDrawRectangle(r.x,r.y,r.w,r.h);
    ofSetColor(255);ofDrawBitmapString("Terrain",r.x+6,r.y+21);
    const char* labels[]={"Top","Up","Down"};
    for(int i=0;i<3;++i){
        bool enabled=i==2?offset<maximum:offset>0;
        float x=r.x+caption+i*buttonWidth;
        ofSetColor(enabled?65:40);ofDrawRectangle(x+2,r.y+2,buttonWidth-4,r.h-4);
        ofSetColor(enabled?255:130);ofDrawBitmapString(labels[i],x+7,r.y+21);
    }
    ofPopStyle();
}
}
