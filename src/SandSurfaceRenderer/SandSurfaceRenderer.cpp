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
/* PROJECT_ATTRIBUTION_V45
 * Original Magic-Sand terrain renderer retains its original authors' notices.
 * Replacement terrain images were generated for this fork with ChatGPT (v39).
 * Grass/rock mixing and snow controls are educational-fork additions.
 * See CREDITS.md, ASSETS.md and docs/assets/image-prompts-and-provenance.json.
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

#include "SandSurfaceRenderer.h"
#include <stdexcept>
#include <cmath>
#include <sstream>
#include "../LengthUnits.h"
#include "../TriggerSettings.h"
#include "../World/AnimalTerrain.h"
#include "../World/WaterRippleSettings.h"

using namespace ofxCSG;

SandSurfaceRenderer::SandSurfaceRenderer(std::shared_ptr<KinectProjector> const& k, std::shared_ptr<ofAppBaseWindow> const& p)
:settingsLoaded(false),
editColorMap(false){
    kinectProjector = k;
    projWindow = p;
}

void SandSurfaceRenderer::setup(bool sdisplayGui){
    ofAddListener(ofEvents().exit, this, &SandSurfaceRenderer::exit);
    
    // Sandbox contourlines
    drawContourLines = true; // Flag if topographic contour lines are enabled
	contourLineDistance = 10.0; // Elevation distance between adjacent topographic contour lines in millimiters
    
    // Initialize the fbos and images
    projResX = projWindow->getWidth();
    projResY = projWindow->getHeight();
    contourLineFramebufferObject.allocate(projResX+1, projResY+1, GL_RGBA);
    contourLineFramebufferObject.begin();
    ofClear(0,0,0,255);
    contourLineFramebufferObject.end();

    //Try to load settings file if possible
    if (loadSettings())
    {
        ofLogVerbose("SandSurfaceRenderer") << "SandSurfaceRenderer.setup(): sandSurfaceRendererSettings.xml loaded " ;
        settingsLoaded = true;
    } else {
        ofLogVerbose("SandSurfaceRenderer") << "SandSurfaceRenderer.setup(): sandSurfaceRendererSettings.xml could not be loaded " ;
    }

    // Load colormap folder and set heightmap
    colorMapPath = "colorMaps/";
    ofDirectory dir(colorMapPath);
    dir.allowExt("xml");
    dir.listDir();
    for(int i = 0; i < dir.size(); i++){
        colorMapFilesList.push_back(dir.getName(i));
    }
    
    bool heighMapFileLoaded = true;
    
    if (settingsLoaded)
        heighMapFileLoaded = heightMap.loadFile(colorMapPath+colorMapFile);
    
    if (!(settingsLoaded && heighMapFileLoaded) && dir.size() > 0)
    {
        heighMapFileLoaded = heightMap.loadFile(colorMapPath+colorMapFilesList[0]);
        colorMapFile = colorMapFilesList[0];
        saveSettings();
        settingsLoaded = true;
    }
    
    if (!heighMapFileLoaded)
    {
        heightMap.createFile(colorMapPath+"HeightColorMap.xml");
        colorMapFile = "HeightColorMap.xml";
        saveSettings();
        settingsLoaded = true;
    }
    
    //Set elevation Min and Max
    elevationMin = -heightMap.getScalarRangeMin();
    elevationMax = -heightMap.getScalarRangeMax();
    
    // Calculate the  height map elevation scaling and offset coefficients
	heightMapScale = (heightMap.getNumEntries()-1)/((elevationMax-elevationMin));
	heightMapOffset = 0.5/heightMap.getNumEntries()-heightMapScale*elevationMin;
    
    // Calculate the contourline fbo scaling and offset coefficients
	contourLineFboScale = elevationMin-elevationMax;
	contourLineFboOffset = elevationMax;
    contourLineFactor = contourLineFboScale/contourLineDistance;
    
	kinectROI = kinectProjector->getKinectROI();

    //setup the mesh
    setupMesh();
    
	// Load shaders
    bool loaded = true;
#ifdef TARGET_OPENGLES
    ofLogVerbose("SandSurfaceRenderer") << "setup(): Loading shadersES2";
	loaded = loaded && elevationShader.load("shaders/shadersES2/elevationShader");
	loaded = loaded && heightMapShader.load("shaders/shadersES2/heightMapShader");
#else
	if(ofIsGLProgrammableRenderer()){
        ofLogVerbose("SandSurfaceRenderer") << "setup(): Loading shadersGL3/elevationShader";
		loaded = loaded && elevationShader.load("shaders/shadersGL3/elevationShader");
        ofLogVerbose("SandSurfaceRenderer") << "setup(): Loading shadersGL3/heightMapShader";
		loaded = loaded && heightMapShader.load("shaders/shadersGL3/heightMapShader");
	}else{
        ofLogVerbose("SandSurfaceRenderer") << "setup(): Loading shadersGL2/elevationShader";
		loaded = loaded && elevationShader.load("shaders/shadersGL2/elevationShader");
        ofLogVerbose("SandSurfaceRenderer") << "setup(): Loading shadersGL2/heightMapShader";
		loaded = loaded && heightMapShader.load("shaders/shadersGL2/heightMapShader");
	}
#endif
    if (!loaded)
    {
        ofLogError("GreatSand") << "setup(): shader not loaded" ;
    }
    
    // Load natural terrain textures
    if (!sandyGrassTexture.load("textures/sandygrass.jpg"))
        ofLogError("SandSurfaceRenderer") << "Could not load sandy grass texture";

    if (!grassTexture.load("textures/grass.jpg"))
        ofLogError("SandSurfaceRenderer") << "Could not load grass texture";

    if (!spottyGrassTexture.load("textures/spottygrass.jpg"))
        ofLogError("SandSurfaceRenderer") << "Could not load spotty grass texture";

    if (!rockTexture.load("textures/rock_diff.jpg"))
        ofLogError("SandSurfaceRenderer") << "Could not load rock texture";

    if (!mountainTexture.load("textures/mountainside.jpeg"))
        ofLogError("SandSurfaceRenderer") << "Could not load mountainside texture";

    if (!snowTexture.load("textures/snow_diff.jpg"))
        ofLogError("SandSurfaceRenderer") << "Could not load snow texture";

    //Prepare fbo
    fboProjWindow.allocate(projResX, projResY, GL_RGBA);
    fboProjWindow.begin();
    ofClear(0,0,0,255);
    fboProjWindow.end();
    
    displayGui = sdisplayGui;
    if (displayGui)
        setupGui();
    
    // Setup range, base plane and conversion matrices
    updateConversionMatrices();
    updateRangesAndBasePlane();
}

void SandSurfaceRenderer::exit(ofEventArgs& e){
    if (saveSettings())
    {
        ofLogVerbose("SandSurfaceRenderer") << "exit(): Settings saved " ;
    } else {
        ofLogVerbose("SandSurfaceRenderer") << "exit(): Settings could not be saved " ;
    }
}

void SandSurfaceRenderer::updateConversionMatrices(){
    stableTerrainHeights.clear();
    // Get conversion matrices
    transposedKinectProjMatrix = kinectProjector->getTransposedKinectProjMatrix();
    transposedKinectWorldMatrix = kinectProjector->getTransposedKinectWorldMatrix();
}

void SandSurfaceRenderer::updateRangesAndBasePlane(){
    stableTerrainHeights.clear();
    // Rebuild the color lookup transform when boundaries move, including when
    // water colors are above zero or the beach lies below zero. This changes
    // the visual map, not the calibrated waterline used by the simulations.
    elevationMin=-heightMap.getScalarRangeMin();
    elevationMax=-heightMap.getScalarRangeMax();
    heightMapScale=(heightMap.getNumEntries()-1)/(elevationMax-elevationMin);
    heightMapOffset=0.5/heightMap.getNumEntries()-heightMapScale*elevationMin;
    contourLineFboScale=elevationMin-elevationMax;
    contourLineFboOffset=elevationMax;
    contourLineFactor=contourLineFboScale/contourLineDistance;
    basePlaneEq = kinectProjector->getBasePlaneEq();
    basePlaneNormal = kinectProjector->getBasePlaneNormal();
    basePlaneOffset = kinectProjector->getBasePlaneOffset();

    // Set the FilteredDepthImage native scale - converted to 0..1 when send to the shader
    kinectProjector->updateNativeScale(basePlaneOffset.z+elevationMax, basePlaneOffset.z+elevationMin);
    
    // Calculate the  FilteredDepthImage scaling and offset coefficients
	FilteredDepthScale = elevationMin-elevationMax;
	FilteredDepthOffset = basePlaneOffset.z+elevationMax;
    
    ofLogVerbose("SandSurfaceRenderer") << "setRangesAndBasePlaneEquation(): basePlaneOffset: " << basePlaneOffset ;
    ofLogVerbose("SandSurfaceRenderer") << "setRangesAndBasePlaneEquation(): basePlaneNormal: " << basePlaneNormal ;
}

void SandSurfaceRenderer::setupMesh(){
    // Initialise mesh
    kinectROI = kinectProjector->getKinectROI();
	ofLogVerbose("SandSurfaceRenderer") << "setupMesh. KinectROI: " << kinectROI;

    meshwidth = kinectROI.width;
    meshheight = kinectROI.height;
    mesh.clear();stableTerrainHeights.clear();

    // XY, texture coordinates and indices remain fixed until ROI changes.
    // Z carries the held material elevation and changes only on accepted edits.
    mesh.setUsage(GL_DYNAMIC_DRAW);

	for (unsigned int y = 0; y < meshheight; y++)
        for(unsigned int x=0;x<meshwidth;x++)
        {
            ofPoint pt = ofPoint(x+kinectROI.x,y+kinectROI.y,1000000.0f)-ofPoint(0.5,0.5,0); // We move of a half pixel to center the color pixel (more beautiful)
            mesh.addVertex(pt); // make a new vertex
mesh.addTexCoord(glm::vec2(pt.x, pt.y));
        }
    for(unsigned int y=0;y<meshheight-1;y++)
        for(unsigned int x=0;x<meshwidth-1;x++)
        {
            mesh.addIndex(x+y*meshwidth);         // 0
            mesh.addIndex((x+1)+y*meshwidth);     // 1
            mesh.addIndex(x+(y+1)*meshwidth);     // 10
            
            mesh.addIndex((x+1)+y*meshwidth);     // 1
            mesh.addIndex((x+1)+(y+1)*meshwidth); // 11
            mesh.addIndex(x+(y+1)*meshwidth);     // 10
        }
}

void SandSurfaceRenderer::update(){
    syncEditorVisibility(); // Before any panel runs the addon's global focus scan.
    // CONTROLS_V3: convert existing list labels without changing selection.
    static unsigned unitsSeen=MagicSandUnits::revision();
    if(unitsSeen!=MagicSandUnits::revision()){
        unitsSeen=MagicSandUnits::revision();
        for(int i=0;i<heightMap.size();++i)colorList->get(i)->setLabel(editorLabel(heightMap.size()-1-i));
        if(textureEditorActive())configureHeightSlider(heightMap.size()-1-selectedColor);
    }

    // The ceiling is adjustable at runtime and is stored in physical mm even
    // when the GUI displays inches. Changing its range does not rescale terrain.
    float ceiling=MagicSandTriggers::values().handFloor;
    if(textureEditorActive() && terrainEditorCeiling!=ceiling){
        terrainEditorCeiling=ceiling;
        configureHeightSlider(heightMap.size()-1-selectedColor);
    }

    // Update Renderer state if needed
	if (kinectProjector->getKinectROI() != kinectROI)
		setupMesh();
    if (kinectProjector->isBasePlaneUpdated())
        updateRangesAndBasePlane();
    if (kinectProjector->isCalibrationUpdated())
        updateConversionMatrices();
    
    // Existing budget: one terrain redraw per 15 app frames (1 Hz at 15 FPS).
    // V78 does not increase camera capture or terrain render frequency.
    static unsigned int terrainFrameCounter = 0;
    terrainFrameCounter++;

    // A drag requests a bounded preview refresh, rather than removing the
    // normal terrain budget or rebuilding continuously while idle.
    bool redrawLook = terrainLookDirty && ofGetElapsedTimef() >= terrainLookNextRedraw;
    if ((terrainFrameCounter % 15) == 0 || redrawLook)
    {
        if (drawContourLines)
            prepareContourLinesFbo();

        drawSandbox();
        terrainLookDirty = false;
        terrainLookNextRedraw = ofGetElapsedTimef() + 0.20f;
    }
    
    // GUI
	if (displayGui) {
		gui->update();
		gui2->update();
        if (editColorMap){
            activeEditor()->update();
            if(!textureEditorActive())colorList->update();
        }
	}
}

void SandSurfaceRenderer::drawMainWindow(float x, float y, float width, float height){
    fboProjWindow.draw(x, y, width, height);
    drawGuiOnly();
}

void SandSurfaceRenderer::drawGuiOnly(){
    if (displayGui) {
        heightMap.getTexture().draw(
            gui2->getPosition().x,
            gui2->getPosition().y + gui2->getHeight(),
            gui2->getWidth(),
            30
        );

        gui->draw();
        gui2->draw();

        if (editColorMap){
            activeEditor()->draw();
            if(!textureEditorActive())colorList->draw();
            else{
                // One selector: its header carries the current texture preview.
                // There is no second picture/name list with an independent scroll.
                int row=heightMap.size()-1-selectedColor;
                ofImage* first=nullptr;ofImage* second=nullptr;
                if(row>=7 && row<=9)first=&sandyGrassTexture;
                else if(row==10){first=&sandyGrassTexture;second=&grassTexture;}
                else if(row==11)first=&grassTexture;
                else if(row==12)first=&spottyGrassTexture;
                else if(row==13 || row==14){first=&spottyGrassTexture;second=&mountainTexture;}
                else if(row==15)first=&mountainTexture;
                else if(row==16 || row==17){first=&mountainTexture;second=&snowTexture;}
                else if(row>=18)first=&snowTexture;
                auto selector=textureEditor->getDropdown("Terrain layer");
                float x=selector->getX()+4,y=selector->getY()+4,w=32,h=30;
                ofPushStyle();ofSetColor(255);
                if(first && first->isAllocated()){
                    if(second && second->isAllocated()){
                        first->draw(x,y,w*.5f,h);second->draw(x+w*.5f,y,w*.5f,h);
                    }else first->draw(x,y,w,h);
                }else{ofSetColor(heightMap[row].color);ofDrawRectangle(x,y,w,h);}
                ofPopStyle();
            }
        }
    }
}

void SandSurfaceRenderer::drawProjectorWindow(){
	fboProjWindow.draw(0,0);
}

void SandSurfaceRenderer::updateStableTerrainHeights(){
    const size_t count=mesh.getNumVertices();
    if(stableTerrainHeights.size()!=count)stableTerrainHeights.assign(count,StableTerrainHeight{});
    const float now=ofGetElapsedTimef();
    for(size_t i=0;i<count;++i){
        auto vertex=mesh.getVertex(i);
        // This reads the camera's EXISTING FILTERED CPU image, not the raw hand
        // snapshot, a new capture, or a GPU readback. The live depth texture still
        // determines calibrated geometry; only material elevation gets a hold.
        float measured=kinectProjector->elevationAtKinectCoord(vertex.x,vertex.y);
        if(!std::isfinite(measured)||measured < -700 || measured>=MagicSandTriggers::values().handFloor){
            if(!stableTerrainHeights[i].valid && vertex.z!=1000000.f){vertex.z=1000000.f;mesh.setVertex(i,vertex);}
            continue;
        }
        float height=stableTerrainHeights[i].update(measured,now);
        if(vertex.z!=height){vertex.z=height;mesh.setVertex(i,vertex);}
    }
}

void SandSurfaceRenderer::drawSandbox() {
    // Publish the same effective boundaries the projector is about to draw.
    // Requested heights hidden by overlaps must not create invisible habitat.
    auto water=MagicSandAnimalTerrain::reference();
    if(textureEditorActive()){
        for(int n=0;n<7;++n)water[n]=heightMap[n].height;
        for(int n=7;n<20;++n)water[n]=heightMap.textureHeights()[n-7];
    }
    MagicSandAnimalTerrain::publish(textureEditorActive(),water);
    updateStableTerrainHeights();
    fboProjWindow.begin();
    ofBackground(0);
    kinectProjector->bind();
    heightMapShader.begin();
    heightMapShader.setUniformMatrix4f("kinectProjMatrix",transposedKinectProjMatrix);
    heightMapShader.setUniformMatrix4f("kinectWorldMatrix",transposedKinectWorldMatrix);
    heightMapShader.setUniform2f("heightColorMapTransformation",ofVec2f(heightMapScale,heightMapOffset));
    heightMapShader.setUniform2f("depthTransformation",ofVec2f(FilteredDepthScale,FilteredDepthOffset));
    heightMapShader.setUniform4f("basePlaneEq", basePlaneEq);
    heightMapShader.setUniformTexture("heightColorMapSampler",heightMap.getTexture(), 2);
    heightMapShader.setUniformTexture("pixelCornerElevationSampler", contourLineFramebufferObject.getTexture(), 3);
    heightMapShader.setUniform1f("contourLineFactor", contourLineFactor);
    heightMapShader.setUniform1i("drawContourLines", drawContourLines);
    // TEXTURE_EDITOR_V52: the visual material scale follows saved layer heights.
    // Physical elevation used by water, animals, gestures and contours is unchanged.
    auto reference=MagicSandTerrainLayers::defaults();
    heightMapShader.setUniform1i("useRealisticTextures",textureEditorActive()?1:0);
    heightMapShader.setUniform1fv("materialHeights",heightMap.textureHeights().data(),13);
    heightMapShader.setUniform1fv("materialReference",reference.data(),13);
    heightMapShader.setUniform1f("foothillRockPercent", foothillRockPercent);
    heightMapShader.setUniform1f("terrainBlendMm",transitionSoftnessPercent*.16f);
    heightMapShader.setUniform1f("snowCoveragePercent", snowCoveragePercent);
    heightMapShader.setUniform1f("snowOpacityPercent", snowOpacityPercent);

    // Natural terrain materials
    heightMapShader.setUniformTexture("sandyGrassTextureSampler",
                                      sandyGrassTexture.getTexture(), 4);

    heightMapShader.setUniformTexture("grassTextureSampler",
                                      grassTexture.getTexture(), 5);

    heightMapShader.setUniformTexture("spottyGrassTextureSampler",
                                      spottyGrassTexture.getTexture(), 6);

    heightMapShader.setUniformTexture("rockTextureSampler",
                                      rockTexture.getTexture(), 7);

    heightMapShader.setUniformTexture("mountainTextureSampler",
                                      mountainTexture.getTexture(), 8);

    heightMapShader.setUniformTexture("snowTextureSampler",
                                      snowTexture.getTexture(), 9);

    heightMapShader.setUniform2f("sandyGrassTextureSize",
                                 sandyGrassTexture.getWidth(),
                                 sandyGrassTexture.getHeight());

    heightMapShader.setUniform2f("grassTextureSize",
                                 grassTexture.getWidth(),
                                 grassTexture.getHeight());

    heightMapShader.setUniform2f("spottyGrassTextureSize",
                                 spottyGrassTexture.getWidth(),
                                 spottyGrassTexture.getHeight());

    heightMapShader.setUniform2f("rockTextureSize",
                                 rockTexture.getWidth(),
                                 rockTexture.getHeight());

    heightMapShader.setUniform2f("mountainTextureSize",
                                 mountainTexture.getWidth(),
                                 mountainTexture.getHeight());

    heightMapShader.setUniform2f("snowTextureSize",
                                 snowTexture.getWidth(),
                                 snowTexture.getHeight());

    mesh.draw();
    heightMapShader.end();
    kinectProjector->unbind();
    fboProjWindow.end();
}

void SandSurfaceRenderer::prepareContourLinesFbo()
{
    contourLineFramebufferObject.begin();
    ofClear(255,255,255, 0);
    kinectProjector->bind();
	elevationShader.begin();
    elevationShader.setUniformMatrix4f("kinectProjMatrix",transposedKinectProjMatrix);
    elevationShader.setUniformMatrix4f("kinectWorldMatrix",transposedKinectWorldMatrix);
    elevationShader.setUniform2f("contourLineFboTransformation",ofVec2f(contourLineFboScale,contourLineFboOffset));
    elevationShader.setUniform2f("depthTransformation",ofVec2f(FilteredDepthScale,FilteredDepthOffset));
    elevationShader.setUniform4f("basePlaneEq", basePlaneEq);
    mesh.draw();
    elevationShader.end();
    kinectProjector->unbind();
    contourLineFramebufferObject.end();
}

void SandSurfaceRenderer::setupGui(){
    // instantiate the modal windows //
    auto theme = make_shared<ofxModalThemeProjKinect>();
    saveModal = make_shared<SaveModal>(theme);
    saveModal->addListener(this, &SandSurfaceRenderer::onSaveModalEvent);
    
    // instantiate the gui //
    gui2 = new ofxDatGui( ofxDatGuiAnchor::TOP_LEFT );
    gui2->addToggle("Contour lines", drawContourLines)->setStripeColor(ofColor::blue);
    MagicSandUnits::track(gui2->addSlider("Lines distance",1,30,contourLineDistance),"Contour lines distance",1,30,contourLineDistance);
    gui2->getSlider("Contour lines distance")->setStripeColor(ofColor::blue);
    gui2->addDropdown("Load Color Map", colorMapFilesList)->setName("Load Color Map");
    gui2->getDropdown("Load Color Map")->setStripeColor(ofColor::yellow);
    // Do not register this slider with LengthUnits: percent must stay percent.
    gui2->addSlider("Foothill rocks %", 0, 100, foothillRockPercent)->setPrecision(1);
    gui2->addSlider("Transition softness %",0,100,transitionSoftnessPercent)->setPrecision(1);
    gui2->addSlider("Snow coverage %", 0, 100, snowCoveragePercent)->setPrecision(1);
    gui2->addSlider("Snow opacity %", 0, 100, snowOpacityPercent)->setPrecision(1);
    gui2->addToggle("Water ripples",MagicSandWaterRipples::enabled());
    gui2->addHeader(":: Display ::", false);

    gui = new ofxDatGui( ofxDatGuiAnchor::NO_ANCHOR );
    gui->setPosition(gui2->getPosition().x, gui2->getPosition().y+gui2->getHeight()+30);
    gui->addButton("Reset colors to color map file")->setName("Reset colors");
    gui->addButton("Save to color map file")->setName("Save");
    gui->addToggle("Edit color map", editColorMap)->setName("Edit");

    gui3 = new ofxDatGui( ofxDatGuiAnchor::NO_ANCHOR );
    MagicSandUnits::track(gui3->addSlider("Height",-300,300,0),"Height",-300,300,0);
    gui3->addColorPicker("ColorPicker", ofColor::black);
    gui3->addButton("Undo color change")->setName("Undo");
    gui3->addButton("Move color up")->setName("Move up");
    gui3->addButton("Move color down")->setName("Move down");
    gui3->addButton("Insert new color after current color");
    gui3->addButton("Remove color");
    
    textureEditor=new ofxDatGui(ofxDatGuiAnchor::NO_ANCHOR);
    std::vector<std::string> layers;
    for(int row=0;row<20;++row)layers.push_back(MagicSandTerrainLayers::name(row));
    textureEditor->addDropdown("Terrain layer",layers)->setName("Terrain layer");
    textureEditor->addLabel("Height")->setName("Terrain readout");
    textureEditor->addTextInput("Set height","")->setName("Exact height");
    textureEditor->addToggle("Pin selected layer",false)->setName("Pin layer");
    textureEditor->addButton("- 1 mm")->setName("Layer down");
    textureEditor->addButton("+ 1 mm")->setName("Layer up");
    textureEditor->addLabel("Linked spacing")->setName("Layer status");
    textureEditor->onTextInputEvent(this,&SandSurfaceRenderer::onTerrainTextInput);
    textureEditor->onToggleEvent(this,&SandSurfaceRenderer::onToggleEvent);
    textureEditor->onButtonEvent(this,&SandSurfaceRenderer::onButtonEvent);
    textureEditor->onDropdownEvent(this,&SandSurfaceRenderer::onDropdownEvent);
    MagicSandUnits::track(textureEditor->addSlider("Layer height",-300,400,0),"Layer height",-300,400,0);
    textureEditor->onSliderEvent(this,&SandSurfaceRenderer::onSliderEvent);
    textureEditor->setAutoDraw(false);

    // once the gui has been assembled, register callbacks to listen for component specific events //
    gui2->onToggleEvent(this, &SandSurfaceRenderer::onToggleEvent);
    gui2->onSliderEvent(this, &SandSurfaceRenderer::onSliderEvent);
    gui2->onDropdownEvent(this, &SandSurfaceRenderer::onDropdownEvent);
    gui->onButtonEvent(this, &SandSurfaceRenderer::onButtonEvent);
    gui->onToggleEvent(this, &SandSurfaceRenderer::onToggleEvent);
    gui3->onButtonEvent(this, &SandSurfaceRenderer::onButtonEvent);
    gui3->onSliderEvent(this, &SandSurfaceRenderer::onSliderEvent);
    gui3->onColorPickerEvent(this, &SandSurfaceRenderer::onColorPickerEvent);
    
	gui->setAutoDraw(false);
	gui2->setAutoDraw(false);
	gui3->setAutoDraw(false);
	
	int pos = find(colorMapFilesList.begin(), colorMapFilesList.end(), colorMapFile) - colorMapFilesList.begin();
    if (pos < colorMapFilesList.size())
        gui2->getDropdown("Load Color Map")->select(pos);
    
    // The addon's default is six-point type. Give terrain controls a readable
    // twelve-point font and roomy rows, without changing the other GUI panels.
    terrainLargeTheme=std::make_shared<ofxDatGuiTheme>();
    terrainLargeTheme->font.size=12;terrainLargeTheme->layout.height=38;
    terrainLargeTheme->layout.labelWidth=60;terrainLargeTheme->layout.labelMargin=8;
    terrainLargeTheme->layout.upperCaseLabels=false;terrainLargeTheme->init();
    gui->setTheme(terrainLargeTheme.get());
    textureEditor->setTheme(terrainLargeTheme.get());
    terrainListTheme=std::make_shared<ofxDatGuiTheme>();
    terrainListTheme->font.size=10;terrainListTheme->layout.height=32;terrainListTheme->layout.labelWidth=0.55f;
    terrainListTheme->layout.upperCaseLabels=false;terrainListTheme->init();
    gui2->setTheme(terrainListTheme.get());
    // add a scroll view to list colors //
    colorList = new MagicSandTerrainTextureList("Colors / terrain layers", 7);
    colorList->setPosition(gui->getPosition().x, gui->getPosition().y+gui->getHeight());
    colorList->setTheme(terrainListTheme.get());
    colorList->onScrollViewEvent(this, &SandSurfaceRenderer::onScrollViewEvent);
    populateColorList();
    gui3->setPosition(colorList->getX(), colorList->getY()+colorList->getHeight());
}

void SandSurfaceRenderer::updateColorListColor(int i, int j){
    ofColor kc = heightMap[j].color;
    if(textureEditorActive()){
        colorList->get(i)->setBackgroundColors(ofColor(42),ofColor(64),ofColor(78));
        colorList->get(i)->setLabelColor(ofColor::white);
        colorList->get(i)->setLabelAlignment(ofxDatGuiAlignment::LEFT);
        ofImage* image=nullptr;ofImage* second=nullptr;
        if(j>=7 && j<=9)image=&sandyGrassTexture;
        else if(j==10){image=&sandyGrassTexture;second=&grassTexture;}
        else if(j==11)image=&grassTexture;
        else if(j==12)image=&spottyGrassTexture;
        else if(j==13 || j==14){image=&spottyGrassTexture;second=&mountainTexture;}
        else if(j==15)image=&mountainTexture;
        else if(j==16 || j==17){image=&mountainTexture;second=&snowTexture;}
        else if(j>=18)image=&snowTexture;
        colorList->preview(i,image,second);
        if(j<7){colorList->get(i)->setBackgroundColors(kc,kc,kc);colorList->get(i)->setLabelColor(kc.getInverted());}
        if(i==selectedColor){colorList->get(i)->setBackgroundColors(ofColor(35,90,120),ofColor(45,110,140),ofColor(45,110,140));colorList->get(i)->setLabelColor(ofColor::white);}
        return;
    }
    ofColor kb = kc;
    float st = (kc.getSaturation() > 10) ? kc.getSaturation()-10 : 0;
    float bt = (kc.getBrightness() < 245) ? kc.getBrightness()+10 : 255;
    kb.setSaturation(st);
    kb.setBrightness(bt);
    colorList->get(i)->setBackgroundColors(kc, kb, kb);
    colorList->get(i)->setLabelColor(kc.getInverted());
}

void SandSurfaceRenderer::populateColorList(){
    syncEditorVisibility();
    gui->getToggle("Edit")->setLabel(textureEditorActive()?"Edit texture layers":"Edit color map");
    gui->getButton("Save")->setLabel(textureEditorActive()?"Save terrain map":"Save to color map file");
    gui->getButton("Reset colors")->setLabel(textureEditorActive()?"Reset layers from map":"Reset colors to color map file");
    colorList->textureMode(textureEditorActive());
    colorList->clear();
    for (int i = 0 ; i < heightMap.size() ; i++){
        int j = heightMap.size()-1-i;
        colorList->add("color");
        updateColorListColor(i, j);
        colorList->get(i)->setLabel(editorLabel(j));
    }
    //Initiate color controls
    selectedColor = 0;
    int j = heightMap.size()-1;
    gui3->getColorPicker("ColorPicker")->setColor(heightMap[j].color);
    undoColor = heightMap[j].color;
    configureHeightSlider(j);
    colorList->get(0)->setLabelAlignment(textureEditorActive()?ofxDatGuiAlignment::LEFT:ofxDatGuiAlignment::CENTER);
}

void SandSurfaceRenderer::onButtonEvent(ofxDatGuiButtonEvent e){
    if(textureEditorActive() && (e.target->is("Layer down") || e.target->is("Layer up"))){
        int row=heightMap.size()-1-selectedColor;
        float step=MagicSandUnits::inches()?1.27f:1.0f;
        applyLinkedHeight(editorHeight(row)+(e.target->is("Layer down")?-step:step));return;
    }
    if(textureEditorActive() && !e.target->is("Save") && !e.target->is("Reset colors"))return;
    if (e.target->is("Save")) {
        saveModal->show();
    } else if (e.target->is("Reset colors")) {
        heightMap.loadFile(colorMapPath+colorMapFile);
        populateColorList();updateRangesAndBasePlane();terrainLookDirty=true;
    } else if (e.target->is("Insert new color after current color")){
        int i = heightMap.size();
        int j = heightMap.size()-1-selectedColor;
        float newheight = (j > 0) ? (heightMap[j-1].height+heightMap[j].height)/2 : heightMap[j].height+1;
        colorList->add("color");
        updateColorListColor(i, j);
        colorList->get(i)->setLabel("Height: "+MagicSandUnits::text(newheight));
        colorList->move(i,selectedColor+1);
        heightMap.addKey(heightMap[j].color, newheight);
        onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList, colorList->get(selectedColor+1), selectedColor+1));
    } else if (e.target->is("Remove color")){
        if (heightMap.size() > 1){
            int j = heightMap.size()-1-selectedColor;
            heightMap.removeKey(j);
            colorList->remove(selectedColor);
            int i = selectedColor;
            if (i == heightMap.size())
                i -= 1;
            selectedColor += 1; // To get i != selectedColor => update
            onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList, colorList->get(i), i));
        }
    } else if (e.target->is("Move up")){
        int i = selectedColor;
        int j = heightMap.size()-1-i;
        if (i>0){
            heightMap.swapKeys(j, j+1);
            updateColorListColor(i, j);
            updateColorListColor(i-1, j+1);
            onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList, colorList->get(i-1), i-1));
       }
    } else if (e.target->is("Move down")){
        int i = selectedColor;
        int j = heightMap.size()-1-i;
        if (j>0){
            heightMap.swapKeys(j, j-1);
            updateColorListColor(i, j);
            updateColorListColor(i+1, j-1);
            onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList, colorList->get(i+1), i+1));
        }
    } else if (e.target->is("Undo")){
        int i = selectedColor;
        int j = heightMap.size()-1-i;
        heightMap.setColorKey(j, undoColor);
        gui3->getColorPicker("ColorPicker")->setColor(undoColor);
        updateColorListColor(i, j);
    }
}

void SandSurfaceRenderer::onToggleEvent(ofxDatGuiToggleEvent e){
    if(e.target->is("Water ripples")){MagicSandWaterRipples::enabled()=e.checked;return;}
    if(e.target->is("Pin layer") && textureEditorActive()){
        int row=heightMap.size()-1-selectedColor;heightMap.pinLayer(row,e.checked);
        configureHeightSlider(row);return;
    }
    if (e.target->is("Contour lines")) {
        drawContourLines = e.checked;
    } else if (e.target->is("Edit")) {
        editColorMap = e.checked;
        syncEditorVisibility();
    }
}

void SandSurfaceRenderer::onColorPickerEvent(ofxDatGuiColorPickerEvent e){
    if (e.target->is("ColorPicker")) {
        int i = selectedColor;
        int j = heightMap.size()-1-i;
        heightMap.setColorKey(j, e.color);
        updateColorListColor(i, j);
    }
}

void SandSurfaceRenderer::onSliderEvent(ofxDatGuiSliderEvent e){
    e.value=MagicSandUnits::read(e);
    if(e.target->is("Transition softness %")){
        if(std::isfinite(e.value)){transitionSoftnessPercent=ofClamp(e.value,0.f,100.f);terrainLookDirty=true;}
    } else if (e.target->is("Foothill rocks %")) {
        if (std::isfinite(e.value)) {
            foothillRockPercent = ofClamp(e.value, 0.0f, 100.0f);
            terrainLookDirty = true;
        }
    } else if (e.target->is("Snow coverage %")) {
        if (std::isfinite(e.value)) {
            snowCoveragePercent = ofClamp(e.value,0.0f,100.0f);
            terrainLookDirty = true;
        }
    } else if (e.target->is("Snow opacity %")) {
        if (std::isfinite(e.value)) {
            snowOpacityPercent = ofClamp(e.value,0.0f,100.0f);
            terrainLookDirty = true;
        }
    } else if (e.target->is("Contour lines distance")) {
        contourLineDistance = e.value;
        contourLineFactor = contourLineFboScale/contourLineDistance;        
    } else if (e.target->is("Height") || e.target->is("Layer height")) {
        int i = selectedColor;
        int j = heightMap.size()-1-i;
        if(!std::isfinite(e.value))return;
        if(textureEditorActive()){
            applyLinkedHeight(e.value);return;
        }else{heightMap.setHeightKey(j,e.value);updateRangesAndBasePlane();}
        // Neighboring levels may have moved too; keep every row accurate.
        for(int n=0;n<heightMap.size();++n)colorList->get(n)->setLabel(editorLabel(heightMap.size()-1-n));
        configureHeightSlider(j);
        terrainLookDirty=true;
    }
}

void SandSurfaceRenderer::onDropdownEvent(ofxDatGuiDropdownEvent e){
    if(e.target==textureEditor->getDropdown("Terrain layer")){
        int row=e.child;
        if(row>=0 && row<20 && textureEditorActive()){
            int i=heightMap.size()-1-row;
            onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList,colorList->get(i),i));
            configureHeightSlider(row);
        }
        return;
    }
    colorMapFile = e.target->getLabel();
    heightMap.loadFile(colorMapPath+e.target->getLabel());
    populateColorList();
    updateRangesAndBasePlane();terrainLookDirty=true;
}

void SandSurfaceRenderer::onScrollViewEvent(ofxDatGuiScrollViewEvent e){
    int i = e.index;
    if (i != selectedColor){
        int j = heightMap.size()-1-i;
        e.target->setLabelAlignment(textureEditorActive()?ofxDatGuiAlignment::LEFT:ofxDatGuiAlignment::CENTER);
        colorList->get(selectedColor)->setLabelAlignment(ofxDatGuiAlignment::LEFT);
        gui3->getColorPicker("ColorPicker")->setColor(heightMap[j].color);
        undoColor = heightMap[j].color;
        selectedColor = i;
        configureHeightSlider(j);
    }
}

void SandSurfaceRenderer::onSaveModalEvent(ofxModalEvent e){
    if (e.type == ofxModalEvent::SHOWN){
        cout << "save modal window is open" << endl;
    }   else if (e.type == ofxModalEvent::HIDDEN){
        cout << "save modal window is closed" << endl;
    }   else if (e.type == ofxModalEvent::CANCEL){
        ofLogVerbose("SandSurfaceRenderer") << "save cancel button pressed: Aborting" ;
    }   else if (e.type == ofxModalEvent::CONFIRM){
        string filen = saveModal->getTextInput();
        std::size_t found = filen.find(".xml");
        if (found == std::string::npos)
            filen += ".xml";
        heightMap.saveFile(colorMapPath+filen);
        colorMapFilesList.push_back(filen);
        gui2->getDropdown("Load Color Map")->setOptions(colorMapFilesList);
        ofLogVerbose("SandSurfaceRenderer") << "save confirm button pressed, filename: " << filen;
    }
}


//TODO: Save additionnal settings

bool SandSurfaceRenderer::loadSettings(){
    string settingsFile = "settings/sandSurfaceRendererSettings.xml";

    ofxXmlSettings xml;

    if (!xml.loadFile(settingsFile))
        return false;

    if (!xml.tagExists("SURFACERENDERERSETTINGS"))
        return false;

    xml.pushTag("SURFACERENDERERSETTINGS");

    colorMapFile = xml.getValue("colorMapFile", "");
    drawContourLines = xml.getValue("drawContourLines", false);
    contourLineDistance = xml.getValue("contourLineDistance", 0.0f);

    xml.popTag();

    return true;
}

bool SandSurfaceRenderer::saveSettings(){
    string settingsFile = "settings/sandSurfaceRendererSettings.xml";

    ofxXmlSettings xml;

    xml.addTag("SURFACERENDERERSETTINGS");
    xml.pushTag("SURFACERENDERERSETTINGS");

    xml.addValue("colorMapFile", colorMapFile);
    xml.addValue("drawContourLines", drawContourLines);
    xml.addValue("contourLineDistance", contourLineDistance);

    xml.popTag();

    return xml.saveFile(settingsFile);
}


// EDGE_GUI_V6: retain the color-map editor inside the display column.
float SandSurfaceRenderer::operatorGuiHeight()const{
    if(!displayGui||!gui||!gui2)return 0;
    return gui2->getHeight()+30+gui->getHeight()+(editColorMap?(textureEditorActive()?0:colorList->getHeight())+activeEditor()->getHeight()+8:0);
}
void SandSurfaceRenderer::placeOperatorGui(float x,float y,float width){
    if(!gui||!gui2)return;
    if(gui2->getWidth()!=width)gui2->setWidth(width,0.55f);gui2->setPosition(x,y);
    if(gui->getWidth()!=width)gui->setWidth(width);gui->setPosition(x,y+gui2->getHeight()+30);
    if(colorList){if(colorList->getWidth()!=width)colorList->setWidth(width);// The addon registers wheel events independently of draw/update/visibility.
        // Move the unused list's hit rectangle away too, so it cannot steal input.
        if(textureEditorActive())colorList->setPosition(-100000,-100000);
        else colorList->setPosition(x,y+gui2->getHeight()+30+gui->getHeight());}
    if(gui3){if(gui3->getWidth()!=width)gui3->setWidth(width);gui3->setPosition(x,y+gui2->getHeight()+30+gui->getHeight()+colorList->getHeight()+8);}
    if(textureEditor){if(textureEditor->getWidth()!=width)textureEditor->setWidth(width,60);textureEditor->setPosition(x,y+gui2->getHeight()+30+gui->getHeight()+8);}
}

// BOX_SETTINGS_V7: include unsaved edits to the color-map height/color keys.
void SandSurfaceRenderer::captureBoxGui(ofJson& j)const{
    j["terrain_texture_profile"]=heightMap.usesRealisticTextures();
    if(textureEditorActive()){
        auto desired=heightMap.preferredHeights();
        for(int n=0;n<20;++n)j["terrain_layer_requested_"+std::to_string(n)]=desired[n];
        j["terrain_layer_anchor"]=heightMap.preferredAnchor();
        const auto& linked=heightMap.linkedState();
        for(int n=0;n<20;++n){j["terrain_layer_pin_"+std::to_string(n)]=linked.pins[n];j["terrain_link_basis_"+std::to_string(n)]=linked.basis[n];}
        j["terrain_link_row"]=linked.row;j["terrain_link_ceiling"]=linked.ceiling;
    }
    for(int n=0;n<13;++n)j["terrain_texture_height_"+std::to_string(n)]=heightMap.textureHeights()[n];
    j["terrain_foothill_rocks_pct"] = foothillRockPercent;
    j["terrain_transition_softness_pct"]=transitionSoftnessPercent;
    j["terrain_snow_coverage_pct"] = snowCoveragePercent;
    j["terrain_snow_opacity_pct"] = snowOpacityPercent;
    j["water_ripples_v60"]=MagicSandWaterRipples::enabled();
    j["terrain_contours"]=drawContourLines;j["terrain_spacing_mm"]=contourLineDistance;j["terrain_map_file"]=colorMapFile;j["terrain_editor"]=editColorMap;j["terrain_selected"]=selectedColor;
    auto keys=heightMap.getKeys();j["terrain_key_count"]=int(keys.size());
    for(int n=0;n<int(keys.size());++n){std::string k="terrain_key_"+std::to_string(n);j[k+"_mm"]=keys[n].height;j[k+"_r"]=int(keys[n].color.r);j[k+"_g"]=int(keys[n].color.g);j[k+"_b"]=int(keys[n].color.b);j[k+"_a"]=int(keys[n].color.a);}
}
void SandSurfaceRenderer::restoreBoxGui(const ofJson& j){
    MagicSandWaterRipples::enabled()=j.value("water_ripples_v60",true);
    if(gui2)gui2->getToggle("Water ripples")->setChecked(MagicSandWaterRipples::enabled());
    float savedSoftness=j.value("terrain_transition_softness_pct",40.f);
    transitionSoftnessPercent=std::isfinite(savedSoftness)?ofClamp(savedSoftness,0.f,100.f):40.f;
    if(gui2)gui2->getSlider("Transition softness %")->setValue(transitionSoftnessPercent);
    float savedRocks = j.value("terrain_foothill_rocks_pct", 34.0f);
    foothillRockPercent = std::isfinite(savedRocks) ? ofClamp(savedRocks,0.0f,100.0f) : 34.0f;
    if (gui2) gui2->getSlider("Foothill rocks %")->setValue(foothillRockPercent);
    float savedCoverage = j.value("terrain_snow_coverage_pct",100.0f);
    float savedOpacity = j.value("terrain_snow_opacity_pct",100.0f);
    snowCoveragePercent = std::isfinite(savedCoverage) ? ofClamp(savedCoverage,0.0f,100.0f) : 100.0f;
    snowOpacityPercent = std::isfinite(savedOpacity) ? ofClamp(savedOpacity,0.0f,100.0f) : 100.0f;
    if (gui2) {
        gui2->getSlider("Snow coverage %")->setValue(snowCoveragePercent);
        gui2->getSlider("Snow opacity %")->setValue(snowOpacityPercent);
    }
    int count=j.value("terrain_key_count",0);std::vector<ofColor> colors;std::vector<double> heights;
    for(int n=0;n<count;++n){std::string k="terrain_key_"+std::to_string(n);heights.push_back(j.value((k+"_mm").c_str(),0.f));colors.emplace_back(j.value((k+"_r").c_str(),0),j.value((k+"_g").c_str(),0),j.value((k+"_b").c_str(),0),j.value((k+"_a").c_str(),255));}
    if(!heightMap.setKeys(colors,heights))throw std::runtime_error("Cannot restore terrain colors");
    colorMapFile=j.value("terrain_map_file",colorMapFile);
    heightMap.setRealisticTextures(j.value("terrain_texture_profile",heightMap.usesRealisticTextures() || colorMapFile=="RealisticTerrain.xml"));
    auto levels=MagicSandTerrainLayers::defaults();
    for(int n=0;n<13;++n)levels[n]=j.value("terrain_texture_height_"+std::to_string(n),levels[n]);
    if(!heightMap.setTextureHeights(levels))heightMap.setTextureHeights(MagicSandTerrainLayers::defaults());
    if(textureEditorActive() && j.contains("terrain_layer_requested_0")){
        auto desired=heightMap.preferredHeights();bool complete=true;
        for(int n=0;n<20;++n){
            std::string key="terrain_layer_requested_"+std::to_string(n);
            if(!j.contains(key)){complete=false;break;}
            desired[n]=j.value(key,desired[n]);
        }
        if(complete)heightMap.setVisualLayers(desired,j.value("terrain_layer_anchor",-1));
    }
    if(textureEditorActive()){
        MagicSandLinkedLayers::State state;
        for(int n=0;n<20;++n){state.pins[n]=j.value("terrain_layer_pin_"+std::to_string(n),state.pins[n]);state.basis[n]=j.value("terrain_link_basis_"+std::to_string(n),0.0f);}
        state.row=j.value("terrain_link_row",-1);state.ceiling=j.value("terrain_link_ceiling",0.0f);
        for(int n=0;n<20;++n)if(!j.contains("terrain_link_basis_"+std::to_string(n)))state.row=-1;
        heightMap.restoreLinkedState(state);
    }
    drawContourLines=j.value("terrain_contours",true);contourLineDistance=ofClamp(j.value("terrain_spacing_mm",10.f),1.f,30.f);editColorMap=j.value("terrain_editor",false);
    updateRangesAndBasePlane();
    if(gui2){gui2->getToggle("Contour lines")->setChecked(drawContourLines);MagicSandUnits::value(gui2->getSlider("Contour lines distance"),contourLineDistance);gui2->getDropdown("Load Color Map")->setLabel(colorMapFile);}
    if(gui){gui->getToggle("Edit")->setChecked(editColorMap);populateColorList();int i=ofClamp(j.value("terrain_selected",0),0,count-1);if(i!=selectedColor)onScrollViewEvent(ofxDatGuiScrollViewEvent(colorList,colorList->get(i),i));}
    if(drawContourLines)prepareContourLinesFbo();drawSandbox();
}

// The basic editor still edits color keys. A realistic profile has seven water
// color keys and thirteen texture anchors, rather than pretending that the old
// twenty color-key elevations are the actual material transitions.
bool SandSurfaceRenderer::textureEditorActive()const{return heightMap.usesRealisticTextures() && heightMap.size()==20;}
float SandSurfaceRenderer::editorHeight(int row)const{return textureEditorActive() && row>=7?heightMap.textureHeights()[row-7]:heightMap[row].height;}
std::string SandSurfaceRenderer::editorLabel(int row)const{
    return textureEditorActive()?std::string("        ")+MagicSandTerrainLayers::name(row):
        std::string("Height: ")+MagicSandUnits::text(editorHeight(row));
}
ofxDatGui* SandSurfaceRenderer::activeEditor()const{return textureEditorActive()?textureEditor:gui3;}
ofxDatGuiSlider* SandSurfaceRenderer::activeHeightSlider()const{return textureEditorActive()?textureEditor->getSlider("Layer height"):gui3->getSlider("Height");}
void SandSurfaceRenderer::configureHeightSlider(int row){
    auto slider=activeHeightSlider();float value=editorHeight(row),low,high;bool enabled=true;
    if(textureEditorActive()){
        float ceiling=MagicSandTriggers::values().handFloor;
        auto limits=heightMap.linkedLimits(row,ceiling);
        enabled=limits.valid;low=limits.low;high=limits.high;
        if(!enabled){low=-ceiling;high=ceiling;}
        textureEditor->getDropdown("Terrain layer")->setLabel(std::string("        ")+MagicSandTerrainLayers::name(row));
        textureEditor->getToggle("Pin layer")->setChecked(heightMap.linkedState().pins[row]);
        textureEditor->getLabel("Terrain readout")->setLabel("Height: "+ofToString(MagicSandUnits::display(value),2)+(MagicSandUnits::inches()?" in":" mm"));
        textureEditor->getTextInput("Exact height")->setLabel(MagicSandUnits::inches()?"Set in":"Set mm");
        textureEditor->getTextInput("Exact height")->setText(ofToString(MagicSandUnits::display(value),2));
        textureEditor->getTextInput("Exact height")->setEnabled(enabled);
        textureEditor->getButton("Layer down")->setLabel(MagicSandUnits::inches()?"- 0.05 in":"- 1 mm");
        textureEditor->getButton("Layer up")->setLabel(MagicSandUnits::inches()?"+ 0.05 in":"+ 1 mm");
        textureEditor->getButton("Layer down")->setEnabled(enabled);textureEditor->getButton("Layer up")->setEnabled(enabled);
        textureEditor->getLabel("Layer status")->setLabel(heightMap.linkedState().pins[row]?"Pinned - unpin to move":enabled?"Linked - 1 mm minimum":"Nearby pins too close");
        for(int i=0;i<heightMap.size();++i)updateColorListColor(i,heightMap.size()-1-i);
        value=ofClamp(value,low,high);
    }else{
        low=row>0?heightMap[row-1].height:value-100.0f;
        high=row<heightMap.size()-1?heightMap[row+1].height:value+100.0f;
    }
    slider->setEnabled(enabled);MagicSandUnits::range(slider,low,high,value);
    if(textureEditorActive()){slider->setPrecision(2,false);slider->setLabel(MagicSandUnits::inches()?"in":"mm");}
}
void SandSurfaceRenderer::applyLinkedHeight(float mm){
    if(!textureEditorActive() || !std::isfinite(mm))return;
    int row=heightMap.size()-1-selectedColor;
    if(heightMap.moveLinkedLayer(row,mm,MagicSandTriggers::values().handFloor)){
        updateRangesAndBasePlane();terrainLookDirty=true;
    }
    configureHeightSlider(row);
}
void SandSurfaceRenderer::onTerrainTextInput(ofxDatGuiTextInputEvent e){
    if(!textureEditorActive() || !e.target->is("Exact height"))return;
    // Commit a complete number; a trailing character or non-finite value is
    // rejected rather than interpreted as a different physical elevation.
    std::istringstream input(e.text);float value;char extra;
    if(!(input>>value) || (input>>extra) || !std::isfinite(value)){
        configureHeightSlider(heightMap.size()-1-selectedColor);return;
    }
    applyLinkedHeight(MagicSandUnits::inches()?value*25.4f:value);
}

// setAutoDraw(false) only stops automatic rendering; ofxDatGui still includes
// that panel in its global hit testing. Both editors occupy the same rectangle.
// Its focus() swaps panels in the hit-test order, so clicking the snow panel
// could promote the undrawn basic editor above the texture editor and capture
// both slider drags and text input. Hide AND disable the unused editor; do not
// force focus each frame, which would interrupt typing in the snow controls.
void SandSurfaceRenderer::syncEditorVisibility(){
    bool editing=displayGui && editColorMap;
    bool textured=textureEditorActive();
    if(gui3){gui3->setVisible(editing && !textured);gui3->setEnabled(editing && !textured);}
    if(textureEditor){textureEditor->setVisible(editing && textured);textureEditor->setEnabled(editing && textured);}
}

// The layer list keeps its own wheel scrolling; the rest of the left column
// scrolls normally. Shift-wheel remains a way to move the entire column here.
bool SandSurfaceRenderer::operatorLayerListContains(int x,int y)const{
    return displayGui && editColorMap && !textureEditorActive() && colorList &&
           x>=colorList->getX() && x<colorList->getX()+colorList->getWidth() &&
           y>=colorList->getY() && y<colorList->getY()+colorList->getHeight();
}
