/* TopoPlay contributor credit — 2026-10-25
 * Todd Taylor and Grayson Taylor — TopoPlay development and testing.
 * This credit applies to TopoPlay contributions; original authors retain
 * credit for their work. Existing copyright and license notices are preserved.
 */
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

#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"
#include "RealisticTerrainLayers.h"
#include "LinkedTerrainLayers.h"

class ColorMap
{
public:
    struct HeightMapKey
    {
        float height;
        ofColor color;
        
        HeightMapKey(float h, ofColor c) : height(h), color(c) {}
        
        bool operator < (const HeightMapKey& hmk) const
        {
            return (height < hmk.height);
        }
    };
    
    ColorMap(void)
    :numEntries(512){
    }
    
    bool setKeys(std::vector<ofColor> colorkeys, std::vector<double> heightkeys); // Set keys
    bool updateColormap(void);    // Update colormap based on stored colorkeys
    bool setColorKey(int key, ofColor color);
    bool setHeightKey(int key, float height);
    MagicSandTerrainLayers::AllLevels preferredHeights()const;
    MagicSandTerrainLayers::AllLevels displayedHeights()const;
    const MagicSandLinkedLayers::State& linkedState()const{return linked;}
    void restoreLinkedState(const MagicSandLinkedLayers::State& state);
    void pinLayer(int row,bool yes){if(row>=0 && row<20){linked.pins[row]=yes;linked.row=-1;}}
    bool moveLinkedLayer(int row,float value,float ceiling);
    MagicSandLinkedLayers::Limits linkedLimits(int row,float ceiling)const;
    int preferredAnchor()const{return layerAnchor;}
    bool setVisualLayers(const MagicSandTerrainLayers::AllLevels& desired,int anchor);
    bool addKey(ofColor color, float height);
    bool removeKey(int key);
    bool swapKeys(int k1, int k2);
    bool loadFile(string path);
    void saveFile(string filename);
    bool createFile(string filename); //create a sample colormap file
    HeightMapKey operator[](int scalar) const; // Return a key
    int size() const;
    ofTexture getTexture(); // return color map texture

    // Utilities
    bool scaleRange(float factor); // Rescale the range
    float getScalarRangeMin(void) const // Returns minimum of scalar value range
    {
        return min;
    }
    float getScalarRangeMax(void) const // Returns maximum of scalar value range
    {
        return max>min?max:min+0.001; // Finite lookup scale when all bands coincide.
    }
    int getNumEntries(void) const // Returns the number of entries in the map
    {
        return numEntries;
    }
    int getNumKeys(void) const // Returns the number of colorkeys in the map
    {
        return heightMapKeys.size();
    }
    std::vector<HeightMapKey> getKeys(void) const // Returns the keys in the colormap
    {
        return heightMapKeys;
    }
    
    bool usesRealisticTextures()const{return realisticTextures;}
    void setRealisticTextures(bool yes){realisticTextures=yes;}
    const MagicSandTerrainLayers::Levels& textureHeights()const{return materialLevels;}
    bool setTextureHeights(const MagicSandTerrainLayers::Levels& levels){
        if(!MagicSandTerrainLayers::valid(levels))return false;
        materialLevels=levels;hasDesiredLevels=false;layerAnchor=-1;linked.row=-1;return true;
    }
private:
    MagicSandLinkedLayers::State linked;
    MagicSandTerrainLayers::AllLevels desiredLevels{};
    bool hasDesiredLevels=false;
    int layerAnchor=-1;
    bool realisticTextures=false;
    MagicSandTerrainLayers::Levels materialLevels=MagicSandTerrainLayers::defaults();
    // Colorkeys
    std::vector<HeightMapKey> heightMapKeys;
    
    //Colormap entries
    int numEntries; // Number of colors in the map
    ofPixels entries; // Array of RGBA entries
    ofImage tex;
    double min, max; // The scalar value range
};
