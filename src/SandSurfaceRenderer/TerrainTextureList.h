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
#include "ofxDatGui.h"
#include <vector>
#include <algorithm>
// Draw thumbnails after the addon's existing scroll view. Crop them numerically
// to the visible viewport instead of modifying the addon or GL scissor state.
// Images are the renderer's resident textures; none is loaded during draw().
class MagicSandTerrainTextureList:public ofxDatGuiScrollView {
public:
    explicit MagicSandTerrainTextureList(const std::string& name,int visible):ofxDatGuiScrollView(name,visible){}
    void textureMode(bool yes){textured=yes;}
    void preview(int row,ofImage* first,ofImage* second=nullptr){
        if(int(previews.size())<=row)previews.resize(row+1);
        previews[row]={first,second};
    }
    void draw()override{
        ofxDatGuiScrollView::draw();if(!textured)return;
        ofPushStyle();ofSetColor(255);
        for(int i=0;i<getNumItems() && i<int(previews.size());++i){
            auto button=get(i);float top=button->getY()+3.0f,h=button->getHeight()-6.0f;
            float clippedTop=std::max(0.0f,top),clippedBottom=std::min(float(getHeight()),top+h);
            if(h<=0 || clippedBottom<=clippedTop)continue;
            auto sample=previews[i];if(!sample.first || !sample.first->isAllocated())continue;
            float x=getX()+4.0f,y=getY()+clippedTop,w=32.0f;
            auto piece=[&](ofImage* image,float px,float width){
                if(!image || !image->isAllocated())return;
                float sy=(clippedTop-top)/h*image->getHeight(),sh=(clippedBottom-clippedTop)/h*image->getHeight();
                image->drawSubsection(px,y,width,clippedBottom-clippedTop,0,sy,image->getWidth(),sh);
            };
            if(sample.second){piece(sample.first,x,w/2);piece(sample.second,x+w/2,w/2);}
            else piece(sample.first,x,w);
        }
        ofPopStyle();
    }
private:
    struct Sample{ofImage* first=nullptr;ofImage* second=nullptr;};
    bool textured=false;std::vector<Sample> previews;
};
