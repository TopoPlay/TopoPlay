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
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <future>
#include <memory>
#include <string>

// CPU settings are applied only on startup or an explicit Apply click.
// No sudo prompts or privileged work occurs on the rendering thread.
// CPU performance percentage limits frequency, not process utilization or
// temperature. Frame-rate control separately reduces recurring rendering work;
// both controls were needed for sustained operation on the tested laptop.
class CpuLimiter {
public:
    float operatorGuiHeight()const{return gui&&visible?gui->getHeight():0;}
    void placeOperatorGui(float x,float y,float width){if(gui){if(gui->getWidth()!=width)gui->setWidth(width);gui->setPosition(x,y);}}

    void setup() {
        try {
            const auto path=ofToDataPath("cpu-limit.json",true);
            if(ofFile::doesFileExist(path)) {
                auto j=ofLoadJson(path);int value=j.value("percent",25);
                selectedFps=ofClamp(j.value("fps",20),15,30);
                if(value>=25&&value<=45)selected=value;
            }
        } catch(const std::exception&) {}
        ofSetFrameRate(selectedFps);
        gui.reset(new ofxDatGui());gui->setAutoDraw(false);
        gui->addHeader("COOLING CONTROLS (L SHOW/HIDE)",false);
        slider=gui->addSlider("CPU speed %",25,45,selected);slider->setPrecision(0);
        fpsSlider=gui->addSlider("Frame rate",15,30,selectedFps);fpsSlider->setPrecision(0);
        gui->addButton("Apply and save");
        gui->addButton("Cool preset: 25% / 20 FPS");
        status=gui->addLabel("Checking CPU limit...");
        gui->addLabel("Higher settings can run hotter");
        gui->onSliderEvent(this,&CpuLimiter::onSlider);
        gui->onButtonEvent(this,&CpuLimiter::onButton);
        startApply();
    }
    void update() {
        if(!gui)return;
        if(visible)gui->update();
        if(job.valid()&&job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            int result=job.get();
            bool applied=result==0&&readInt("max_perf_pct")==pending&&readInt("no_turbo")==1;
            if(applied) {
                try {ofJson j;j["percent"]=pending;j["fps"]=pendingFps;ofSavePrettyJson(ofToDataPath("cpu-limit.json",true),j);message="";}
                catch(const std::exception&) {message="Applied; could not save";}
            } else message="Apply failed; check terminal";
            nextStatus=0;
        }
        float now=ofGetElapsedTimef();
        if(now>=nextStatus) {
            nextStatus=now+2;
            int limit=readInt("max_perf_pct"),turbo=readInt("no_turbo");
            if(job.valid())status->setLabel("Applying "+std::to_string(pending)+"%...");
            else if(!message.empty())status->setLabel(message);
            else if(limit<0||turbo<0)status->setLabel("CPU controls unavailable");
            else status->setLabel("Active "+std::to_string(limit)+"% / "+std::to_string(appliedFps)+" FPS / Turbo "+(turbo==1?"OFF":"ON"));
        }
    }
    void draw() {
        if(!gui||!visible)return;
        /* EDGE_GUI_V6: position assigned by main layout */
        gui->draw();
    }
    bool keyPressed(int key) {
        if(key=='l'||key=='L'){visible=!visible;return true;}return false;
    }
private:
    std::unique_ptr<ofxDatGui> gui;
    ofxDatGuiSlider* slider=nullptr;
    ofxDatGuiSlider* fpsSlider=nullptr;
    ofxDatGuiLabel* status=nullptr;
    std::future<int> job;
    bool visible=true;
    int selected=25,pending=25;
    int selectedFps=20,pendingFps=20,appliedFps=20;
    float nextStatus=0;
    std::string message;
    static int readInt(const char* name) {
        std::ifstream input(std::string("/sys/devices/system/cpu/intel_pstate/")+name);
        int value=-1;if(!(input>>value))return -1;return value;
    }
    void startApply() {
        if(job.valid())return;
        pending=selected;pendingFps=selectedFps;appliedFps=pendingFps;
        ofSetFrameRate(appliedFps);message="";nextStatus=0;
        const std::string command="/usr/bin/sudo -n -- /usr/local/libexec/magicsand-cpu-limit "+std::to_string(pending);
        job=std::async(std::launch::async,[command](){return std::system(command.c_str());});
    }
    void onSlider(ofxDatGuiSliderEvent e) {
        if(!std::isfinite(e.value))return;
        if(e.target==slider)selected=int(std::round(ofClamp(e.value,25.0f,45.0f)));
        else if(e.target==fpsSlider)selectedFps=int(std::round(ofClamp(e.value,15.0f,30.0f)));
    }
    void onButton(ofxDatGuiButtonEvent e) {
        if(e.target->is("Cool preset: 25% / 20 FPS")) {selected=25;slider->setValue(25);selectedFps=20;fpsSlider->setValue(20);startApply();}
        else if(e.target->is("Apply and save"))startApply();
    }
};
