/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
/***********************************************************************
ColorMap - ColorMap takes care of the colorMaps.
Copyright (c) 2016 Thomas Wolf
--- Adapted from Oliver Kreylos Vrui GLColorMap:
Copyright (c) 1999-2012 Oliver Kreylos

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Magic Sand; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "ColorMap.h"
#include <cmath>

bool ColorMap::updateColormap() {
    if (entries.isAllocated())
        entries.clear();
    entries.allocate(numEntries, 1, 3);

    /* Evaluate the color function: */
    for(int i=0;i<numEntries;++i)
    {
        /* Calculate the key value for this color map entry: */
        double val=double(i)*(heightMapKeys.back().height-heightMapKeys.front().height)/double(numEntries-1)+heightMapKeys.front().height;

        /* Find the piecewise linear segment of the color function containing the key value using binary search: */
        int l = 0;
        int r = heightMapKeys.size()-1;
        while(r-l>1)
        {
            /* Enforce the invariant keys[l]<=val<keys[r]: */
            int m = (l+r)>>1;
            if(heightMapKeys[m].height<=val)
                l=m;
            else
                r=m;
        }

        /* Evaluate the linear segment: */
        if(r<numEntries)
        {
            /* Interpolate linearly: */
            // A collapsed band is a sharp transition, not a division by zero.
            float span=heightMapKeys[r].height-heightMapKeys[l].height;
            float w=span>0 ? ofClamp(float((val-heightMapKeys[l].height)/span),0.0f,1.0f) : 1.0f;
            ofColor tempcol = heightMapKeys[l].color*(1.0f-w)+heightMapKeys[r].color*w;
            entries.setColor(i,0,tempcol);
        }
        else
        {
            /* There is nothing to the right of the last key, so no need to interpolate: */
            entries.setColor(i,0,heightMapKeys.back().color);
        }
    }
    tex.setFromPixels(entries);
    return true;
}

bool ColorMap::scaleRange(float factor)
{
    for (auto k : heightMapKeys)
        k.height *= factor;
    min *= factor;
    max *= factor;
    return true;
}

bool ColorMap::setColorKey(int key, ofColor color){
    heightMapKeys[key].color = color;
    return updateColormap();
}

MagicSandTerrainLayers::AllLevels ColorMap::preferredHeights()const{
    if(hasDesiredLevels)return desiredLevels;
    MagicSandTerrainLayers::AllLevels result{};
    if(heightMapKeys.size()!=20)return result;
    for(int n=0;n<7;++n)result[n]=heightMapKeys[n].height;
    for(int n=7;n<20;++n)result[n]=materialLevels[n-7];
    return result;
}

MagicSandTerrainLayers::AllLevels ColorMap::displayedHeights()const{
    MagicSandTerrainLayers::AllLevels h{};
    if(heightMapKeys.size()!=20)return h;
    for(int n=0;n<7;++n)h[n]=heightMapKeys[n].height;
    for(int n=7;n<20;++n)h[n]=materialLevels[n-7];
    return h;
}
void ColorMap::restoreLinkedState(const MagicSandLinkedLayers::State& state){
    linked=state;
    if(linked.row<0 || linked.row>=20 || !std::isfinite(linked.ceiling) || linked.ceiling<=0 || !MagicSandTerrainLayers::finite(linked.basis))linked.row=-1;
}
MagicSandLinkedLayers::Limits ColorMap::linkedLimits(int row,float ceiling)const{
    auto base=linked.row==row && linked.ceiling==ceiling?linked.basis:displayedHeights();
    return MagicSandLinkedLayers::limits(base,linked.pins,row,ceiling);
}
bool ColorMap::moveLinkedLayer(int row,float value,float ceiling){
    auto session=linked;
    if(session.row!=row || session.ceiling!=ceiling){session.basis=displayedHeights();session.row=row;session.ceiling=ceiling;}
    MagicSandTerrainLayers::AllLevels result;
    if(!MagicSandLinkedLayers::move(session.basis,session.pins,row,value,ceiling,result))return false;
    if(!setVisualLayers(result,-1))return false;
    linked=session;return true;
}

bool ColorMap::setVisualLayers(const MagicSandTerrainLayers::AllLevels& desired,int anchor){
    if(heightMapKeys.size()!=20 || !MagicSandTerrainLayers::finite(desired) || anchor < -1 || anchor>=20)return false;
    auto visible=MagicSandTerrainLayers::resolve(desired,anchor);
    desiredLevels=desired;hasDesiredLevels=true;layerAnchor=anchor;linked.row=-1;
    // Assign in place, rather than sorting: colors must keep their layer names
    // when neighboring bands collapse to the same elevation.
    for(int n=0;n<20;++n)heightMapKeys[n].height=visible[n];
    for(int n=7;n<20;++n)materialLevels[n-7]=visible[n];
    min=heightMapKeys.front().height;max=heightMapKeys.back().height;
    return updateColormap();
}

bool ColorMap::setHeightKey(int key, float height){
    heightMapKeys[key].height = height;
    
    std::sort(heightMapKeys.begin(), heightMapKeys.end());
    
    min = heightMapKeys.front().height;
    max = heightMapKeys.back().height;
    return updateColormap();
}

bool ColorMap::addKey(ofColor color, float height){
    heightMapKeys.push_back(HeightMapKey(height,color));
    
    std::sort(heightMapKeys.begin(), heightMapKeys.end());
    
    min = heightMapKeys.front().height;
    max = heightMapKeys.back().height;
    return updateColormap();
}

bool ColorMap::removeKey(int key){
    heightMapKeys.erase(heightMapKeys.begin()+key);
    min = heightMapKeys.front().height;
    max = heightMapKeys.back().height;
    return updateColormap();
}

bool ColorMap::swapKeys(int k1, int k2){
    ofColor tmp = heightMapKeys[k1].color;
    heightMapKeys[k1].color = heightMapKeys[k2].color;
    heightMapKeys[k2].color = tmp;
    return updateColormap();
}

ColorMap::HeightMapKey ColorMap::operator[](int scalar) const
{
    return heightMapKeys[scalar];
}

ofTexture ColorMap::getTexture(void)  // return color map texture
{
    return tex.getTexture();
}

int ColorMap::size() const
{
    return heightMapKeys.size();
}

bool ColorMap::loadFile(string filename) {
    hasDesiredLevels=false;layerAnchor=-1;linked=MagicSandLinkedLayers::State{};
    MagicSandLinkedLayers::State loadedLinked;
    MagicSandTerrainLayers::AllLevels requested{};bool haveRequested=false;int anchor=-1;
    heightMapKeys.clear();
    ofxXmlSettings settings;
    if(settings.loadFile(filename)){
        settings.pushTag("keys");
        realisticTextures=settings.getValue("textureProfile",std::string(""))=="realistic-v1" || ofFilePath::getFileName(filename)=="RealisticTerrain.xml";
        auto levels=MagicSandTerrainLayers::defaults();
        if(settings.tagExists("textureLevels")){
            settings.pushTag("textureLevels");
            for(int n=0;n<13;++n)levels[n]=settings.getValue("height",levels[n],n);
            settings.popTag();
        }
        materialLevels=MagicSandTerrainLayers::valid(levels)?levels:MagicSandTerrainLayers::defaults();
        if(settings.tagExists("textureRequested")){
            settings.pushTag("textureRequested");
            anchor=settings.getValue("anchor",-1);
            haveRequested=settings.getNumTags("height")==20;
            for(int n=0;n<20;++n)requested[n]=settings.getValue("height",0.0f,n);
            settings.popTag();
        }
        if(settings.tagExists("linkedEditor")){
            settings.pushTag("linkedEditor");
            for(int n=0;n<20;++n)loadedLinked.pins[n]=settings.getValue("pin",int(loadedLinked.pins[n]),n)!=0;
            loadedLinked.row=settings.getValue("row",-1);
            loadedLinked.ceiling=settings.getValue("ceiling",0.0f);
            if(settings.getNumTags("basis")!=20)loadedLinked.row=-1;
            for(int n=0;n<20;++n)loadedLinked.basis[n]=settings.getValue("basis",0.0f,n);
            settings.popTag();
        }
        int numberOfSavedPoints = settings.getNumTags("key");
        for(int i = 0; i < numberOfSavedPoints; i++){
            settings.pushTag("key", i);
            
            ofPoint p;
            float h = settings.getValue("height", 0);
            char r = settings.getValue("color-r", 0);
            char g = settings.getValue("color-g", 0);
            char b = settings.getValue("color-b", 0);
            ofColor c = ofColor(r, g, b);
            heightMapKeys.push_back(HeightMapKey(h, c));
            settings.popTag();
        }
        
        settings.popTag(); //pop position
    }
    else{
        ofLogError("HightMapKey file did not load!");
        return false;
    }

    min = heightMapKeys.front().height;
    max = heightMapKeys.back().height;
    
    bool applied=realisticTextures && haveRequested && setVisualLayers(requested,anchor);
    restoreLinkedState(loadedLinked);
    return applied || updateColormap();
}

void ColorMap::saveFile(string filename) {
    ofxXmlSettings positions;
    positions.addTag("keys");
    positions.pushTag("keys");
    if(realisticTextures){
        positions.addValue("textureProfile",std::string("realistic-v1"));
        positions.addTag("textureLevels");positions.pushTag("textureLevels");
        for(float value:materialLevels)positions.addValue("height",value);
        positions.popTag();
        if(hasDesiredLevels){
            positions.addTag("textureRequested");positions.pushTag("textureRequested");
            positions.addValue("anchor",layerAnchor);
            for(float h:desiredLevels)positions.addValue("height",h);
            positions.popTag();
        }
    }
    if(realisticTextures){
        positions.addTag("linkedEditor");positions.pushTag("linkedEditor");
        for(bool pin:linked.pins)positions.addValue("pin",int(pin));
        positions.addValue("row",linked.row);positions.addValue("ceiling",linked.ceiling);
        for(float h:linked.basis)positions.addValue("basis",h);
        positions.popTag();
    }
    //points is a vector<ofPoint> that we want to save to a file
    for(int i = 0; i < heightMapKeys.size(); i++){
        //each position tag represents one point
        positions.addTag("key");
        positions.pushTag("key",i);
        //so set the three values in the file
        positions.addValue("height", heightMapKeys[i].height);
        positions.addValue("color-r", heightMapKeys[i].color.r);
        positions.addValue("color-g", heightMapKeys[i].color.g);
        positions.addValue("color-b", heightMapKeys[i].color.b);
        positions.popTag();//pop position
    }
    positions.popTag(); //pop position
    positions.saveFile(filename);
}

bool ColorMap::createFile(string filename) {
    realisticTextures=false;materialLevels=MagicSandTerrainLayers::defaults();hasDesiredLevels=false;layerAnchor=-1;
    heightMapKeys.clear();
    
    int numKeys;
    {
        heightMapKeys.push_back( HeightMapKey(-220.0,ofColor(0, 0,  0)));
        heightMapKeys.push_back( HeightMapKey(-200.0,ofColor(0, 0,  80)));
        heightMapKeys.push_back( HeightMapKey(-170.0,ofColor(0,  30, 100)));
        heightMapKeys.push_back( HeightMapKey(-150.0,ofColor(0,  50, 102)));
        heightMapKeys.push_back( HeightMapKey(-125,ofColor(19, 108, 160)));
        heightMapKeys.push_back( HeightMapKey(-7.5,ofColor(24, 140, 205)));
        heightMapKeys.push_back( HeightMapKey(-2.5,ofColor(135, 206, 250)));
        heightMapKeys.push_back( HeightMapKey(-0.5,ofColor(176, 226, 255)));
        heightMapKeys.push_back( HeightMapKey(0.0,ofColor(0,  97,  71)));
        heightMapKeys.push_back( HeightMapKey(2.5,ofColor(16, 122,  47)));
        heightMapKeys.push_back( HeightMapKey(25,ofColor(232, 215, 125)));
        heightMapKeys.push_back( HeightMapKey(60,ofColor(161,  67,   0)));
        heightMapKeys.push_back( HeightMapKey(90,ofColor(130,  30,  30)));
        heightMapKeys.push_back( HeightMapKey(140,ofColor(161, 161, 161)));
        heightMapKeys.push_back( HeightMapKey(200,ofColor(206, 206, 206)));
        heightMapKeys.push_back( HeightMapKey(220,ofColor(255, 255, 255)));
    }

    ofxXmlSettings positions;
    positions.addTag("keys");
    positions.pushTag("keys");
    //points is a vector<ofPoint> that we want to save to a file
    for(int i = 0; i < heightMapKeys.size(); i++){
        //each position tag represents one point
        positions.addTag("key");
        positions.pushTag("key",i);
        //so set the three values in the file
        positions.addValue("height", heightMapKeys[i].height);
        positions.addValue("color-r", heightMapKeys[i].color.r);
        positions.addValue("color-g", heightMapKeys[i].color.g);
        positions.addValue("color-b", heightMapKeys[i].color.b);
        positions.popTag();//pop position
    }
    positions.popTag(); //pop position
    positions.saveFile(filename);
    
    min = heightMapKeys.front().height;
    max = heightMapKeys.back().height;
    
    return updateColormap();
}

// BOX_SETTINGS_V7: replace the complete map only after validation.
bool ColorMap::setKeys(std::vector<ofColor> colors,std::vector<double> heights){
    if(colors.size()!=heights.size()||heights.size()<2||heights.size()>512)return false;
    std::vector<HeightMapKey> keys;keys.reserve(heights.size());
    for(size_t i=0;i<heights.size();++i){if(!std::isfinite(heights[i]))return false;keys.emplace_back(float(heights[i]),colors[i]);}
    std::stable_sort(keys.begin(),keys.end()); // Preserve named water colors when levels coincide.
    for(size_t i=1;i<keys.size();++i)if(keys[i].height<keys[i-1].height)return false;
    hasDesiredLevels=false;layerAnchor=-1;linked.row=-1;
    heightMapKeys.swap(keys);min=heightMapKeys.front().height;max=heightMapKeys.back().height;return updateColormap();
}
