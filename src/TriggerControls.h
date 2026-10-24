/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ofMain.h"
#include "ofxDatGui.h"
#include "TriggerSettings.h"
#include "LengthUnits.h"
#include <memory>
#include <string>

class TriggerControls {
public:
    float operatorGuiHeight()const{return gui&&visible?gui->getHeight():0;}
    void placeOperatorGui(float x,float y,float width){if(gui){if(gui->getWidth()!=width)gui->setWidth(width);gui->setPosition(x,y);}}

    void setup() {
        auto v=MagicSandTriggers::values();
        try {
            const auto path=ofToDataPath("trigger-settings.json",true);
            if(ofFile::doesFileExist(path)) {
                auto j=ofLoadJson(path);
                v.volcanoPeak=j.value("volcano_peak_mm",0.0f);
                v.handFloor=j.value("hand_floor_mm",280.0f);
                v.ringScale=j.value("button_ring_scale",1.5f);
                v.buttonHold=j.value("button_hold_seconds",1.0f);
            }
        } catch(const std::exception&) {message="Could not load; using defaults";v=MagicSandTriggers::Values();}
        bool raised=MagicSandTriggers::apply(v);pending=MagicSandTriggers::values();
        if(raised)message="Peak lowered beneath ceiling";
        gui.reset(new ofxDatGui());gui->setAutoDraw(false);
        gui->addHeader("TRIGGER SETTINGS (H SHOW/HIDE)",false);
        peak=MagicSandUnits::track(gui->addSlider("Volcano peak",0,500,pending.volcanoPeak),"Volcano peak",0,500,pending.volcanoPeak);
        hand=MagicSandUnits::track(gui->addSlider("Landscape ceiling",50,600,pending.handFloor),"Landscape ceiling",50,600,pending.handFloor);
        gui->addToggle("Measurements in inches",MagicSandUnits::inches());
        gui->onToggleEvent(this,&TriggerControls::onToggle);
        hold=gui->addSlider("Button hold sec",.25f,3,pending.buttonHold);hold->setPrecision(2);
        ring=gui->addSlider("Button ring size",.8f,2.5f,pending.ringScale);ring->setPrecision(2);
        gui->addButton("Apply and save triggers");
        gui->addButton("Restore trigger defaults");
        gui->addLabel("Peak 0 = existing mound rules");
        live=gui->addLabel("Volcano peak: scanning terrain");
        status=gui->addLabel(message);
        gui->onSliderEvent(this,&TriggerControls::onSlider);
        gui->onButtonEvent(this,&TriggerControls::onButton);
    }
    void update() {
        if(!gui)return;
        if(visible)gui->update();
        if(pending.handFloor!=MagicSandTriggers::values().handFloor){pending.handFloor=MagicSandTriggers::values().handFloor;pending.volcanoPeak=MagicSandTriggers::values().volcanoPeak;MagicSandUnits::value(hand,pending.handFloor);MagicSandUnits::value(peak,pending.volcanoPeak);}
        if(ofGetElapsedTimef()>=nextStatus) {
            nextStatus=ofGetElapsedTimef()+.5f;
            float height=MagicSandTriggers::observedPeak();
            live->setLabel(std::isfinite(height)?"Observed peak: "+MagicSandUnits::text(height):"Volcano peak: scanning terrain");
            status->setLabel(message);
        }
    }
    void draw() {
        if(!gui||!visible)return;
        // Middle column between projector-color and cooling controls on
        // the normal 768-pixel-high operator window. P/L/H toggle panels.
        /* EDGE_GUI_V6: position assigned by main layout */
        gui->draw();
    }
    bool keyPressed(int key) {
        if(key=='h'||key=='H'){visible=!visible;return true;}return false;
    }
private:
    std::unique_ptr<ofxDatGui> gui;
    ofxDatGuiSlider *peak=nullptr,*hand=nullptr,*hold=nullptr,*ring=nullptr;
    ofxDatGuiLabel *live=nullptr,*status=nullptr;
    MagicSandTriggers::Values pending;
    bool visible=true;
    float nextStatus=0;
    std::string message="Adjust, then Apply and save";
    void onToggle(ofxDatGuiToggleEvent e){if(e.target->is("Measurements in inches"))MagicSandUnits::setInches(e.checked);}
    void onSlider(ofxDatGuiSliderEvent e) {
        if(!std::isfinite(e.value))return;
        if(e.target==peak)pending.volcanoPeak=MagicSandUnits::read(e);
        else if(e.target==hand){MagicSandTriggers::setCeiling(MagicSandUnits::read(e));pending.handFloor=MagicSandTriggers::values().handFloor;pending.volcanoPeak=MagicSandTriggers::values().volcanoPeak;MagicSandUnits::value(peak,pending.volcanoPeak);}
        else if(e.target==ring)pending.ringScale=e.value;
        else if(e.target==hold)pending.buttonHold=e.value;
        message="Unsaved trigger changes";nextStatus=0;
    }
    void onButton(ofxDatGuiButtonEvent e) {
        if(e.target->is("Restore trigger defaults"))pending=MagicSandTriggers::Values();
        else if(!e.target->is("Apply and save triggers"))return;
        bool raised=MagicSandTriggers::apply(pending);pending=MagicSandTriggers::values();
        MagicSandUnits::value(peak,pending.volcanoPeak);MagicSandUnits::value(hand,pending.handFloor);hold->setValue(pending.buttonHold);ring->setValue(pending.ringScale);
        try {
            ofJson j;j["volcano_peak_mm"]=pending.volcanoPeak;j["hand_floor_mm"]=pending.handFloor;
            j["button_hold_seconds"]=pending.buttonHold;j["button_ring_scale"]=pending.ringScale;
            ofSavePrettyJson(ofToDataPath("trigger-settings.json",true),j);
            message=raised?"Saved; peak lowered beneath ceiling":"Trigger settings saved";
        } catch(const std::exception&) {message="Applied; settings save failed";}
        nextStatus=0;
    }
};
