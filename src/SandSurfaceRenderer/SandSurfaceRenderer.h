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
SandSurfaceRenderer - SandSurfaceRenderer compute and display the colors
on the sand based on the elevation and the colormap.
Copyright (c) 2016 Thomas Wolf

--- Adapted from Oliver Kreylos SurfaceRenderer:
Copyright (c) 2012-2015 Oliver Kreylos

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

#ifndef __GreatSand__SandSurfaceRenderer__
#define __GreatSand__SandSurfaceRenderer__

#include <iostream>
#include "ofMain.h"
#include "../StableTerrainHeight.h"
#include "../KinectProjector/KinectProjector.h"
#include "ColorMap.h"
#include "TerrainTextureList.h"
#include "ofxXmlSettings.h"

class SaveModal : public ofxModalWindow
{
public:
    SaveModal(std::shared_ptr<ofxModalTheme> theme){
        setTheme(theme);
        setTitle("Save Color Map File");
        textInput = addComponent(new ofxDatGuiTextInput("file name", ""));
        addButton("Cancel");
        ofxDatGuiButton* closeButton = getButton(0);
        closeButton->setLabel("Save");
        closeButton->setLabelColor(ofColor::fromHex(0xffffff));
        closeButton->setBackgroundColors(
                                         ofColor::fromHex(0x337ab7),     // normal //
                                         ofColor::fromHex(0x286090),     // on mouse over //
                                         ofColor::fromHex(0x1f4c73));      // on mouse down //
        closeButton->setBorder(ofColor::fromHex(0x1f4c73), 1);
        autoSize();
    }
    void show() {
        textInput->setFocused(true);
        ofxModalWindow::show();
    }
    string getTextInput(){
        return textInput->getText();
    }

private:
    ofxDatGuiTextInput* textInput;
};

class SandSurfaceRenderer {
public:
    SandSurfaceRenderer(std::shared_ptr<KinectProjector> const& k, std::shared_ptr<ofAppBaseWindow> const& p);
    
    // Main loop function
    void setup(bool sdisplayGui);
    void update();
    void drawMainWindow(float x, float y, float width, float height);
    void drawProjectorWindow();
    void drawGuiOnly();
    void captureBoxGui(ofJson& j)const;void restoreBoxGui(const ofJson& j);
    bool saveBoxNativeSettings(){return saveSettings();} // BOX_SETTINGS_V7
    float operatorGuiHeight()const;
    bool operatorLayerListContains(int x,int y)const;
    void placeOperatorGui(float x,float y,float width); // EDGE_GUI_V6
    
    // Gui and events functions
    void setupGui();
    void onButtonEvent(ofxDatGuiButtonEvent e);
    void onToggleEvent(ofxDatGuiToggleEvent e);
    void onSliderEvent(ofxDatGuiSliderEvent e);
    void onColorPickerEvent(ofxDatGuiColorPickerEvent e);
    void onDropdownEvent(ofxDatGuiDropdownEvent e);
    void onScrollViewEvent(ofxDatGuiScrollViewEvent e);
    void onSaveModalEvent(ofxModalEvent e);
    void exit(ofEventArgs& e);
   
private:
    // Private methods
    void setupMesh();
    void updateConversionMatrices();
    void updateRangesAndBasePlane();
    void drawSandbox();
    void prepareContourLinesFbo();
    void updateColorListColor(int i, int j);
    void populateColorList();
    bool textureEditorActive()const;
    float editorHeight(int row)const;
    std::string editorLabel(int row)const;
    ofxDatGui* activeEditor()const;
    ofxDatGuiSlider* activeHeightSlider()const;
    void configureHeightSlider(int row);
    void applyLinkedHeight(float mm);
    void onTerrainTextInput(ofxDatGuiTextInputEvent e);
    std::shared_ptr<ofxDatGuiTheme> terrainLargeTheme,terrainListTheme;
    void syncEditorVisibility();
    float terrainEditorCeiling=-1.0f;
    bool loadSettings();
    bool saveSettings();
    
    // shared pointers
    std::shared_ptr<KinectProjector> kinectProjector;
    std::shared_ptr<ofAppBaseWindow> projWindow;
    bool settingsLoaded;
    
    // Projector Resolution
    int projResX, projResY;
	ofRectangle kinectROI;

    // Conversion matrices
    ofMatrix4x4                 transposedKinectProjMatrix;
    ofMatrix4x4                 transposedKinectWorldMatrix;

    // Mesh
    // Terrain geometry is static between ROI changes.
    // Keep it resident in GPU buffers instead of re-uploading
    // a regular ofMesh every draw.
    ofVboMesh mesh;
    std::vector<StableTerrainHeight> stableTerrainHeights;
    void updateStableTerrainHeights();
    int meshwidth;          //Mesh size
    int meshheight;
    
    // Shaders
    ofShader elevationShader;
    ofShader heightMapShader;

    // This percentage is an appearance control, independent of mm/inches.
    // The preferred default is 34; endpoints give grass/full stone in
    // the foothill band. Save box settings persists it with terrain controls.
    float foothillRockPercent = 34.0f;
    float transitionSoftnessPercent = 40.0f; // V73: visual blend width, not layer positions.
    // Coverage changes where snow lies; opacity changes how much rock shows
    // through it. Keep independent controls and leave the base elevation fixed.
    float snowCoveragePercent = 100.0f;
    float snowOpacityPercent = 100.0f;
    bool terrainLookDirty = false;
    float terrainLookNextRedraw = 0.0f;

    // Natural terrain textures
    ofImage sandyGrassTexture;
    ofImage grassTexture;
    ofImage spottyGrassTexture;

    ofImage rockTexture;
    ofImage mountainTexture;
    ofImage snowTexture;
    
    // FBos
    ofFbo   fboProjWindow;    
    ofFbo   contourLineFramebufferObject;

    // Base plane
    ofVec3f basePlaneNormal, basePlaneNormalBack;
    ofVec3f basePlaneOffset, basePlaneOffsetBack;
    ofVec4f basePlaneEq; // Base plane equation in GLSL-compatible format

    // Colormap
    string colorMapPath;
    string colorMapFile;
    std::vector<string> colorMapFilesList;
    ColorMap    heightMap;
    std::vector<ColorMap::HeightMapKey> heightMapKeys;
    
	float heightMapScale,heightMapOffset; // Scale and offset values to convert from elevation to height color map texture coordinates
    float contourLineFboScale, contourLineFboOffset; // Scale and offset values to convert depth from contourline shader values to real values
	float FilteredDepthScale,FilteredDepthOffset; // Scale and offset values to convert depth from normalized shader values to real values
    float elevationMin, elevationMax;
    
    // Contourlines
    float contourLineDistance, contourLineFactor;
    bool drawContourLines; // Flag if topographic contour lines are enabled
    
    // GUI Main interface and Modal
    bool displayGui;
    bool editColorMap;
    ofxDatGui* gui;
    ofxDatGui* gui2;
    ofxDatGui* gui3=nullptr;
    MagicSandTerrainTextureList* colorList;
    ofxDatGui* textureEditor=nullptr;
    int selectedColor;
    shared_ptr<SaveModal> saveModal;
    ofColor undoColor;
};

#endif /* defined(__GreatSand__SandSurfaceRenderer__) */
