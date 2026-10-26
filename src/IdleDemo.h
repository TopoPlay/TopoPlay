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
#include "ofxDatGui.h"
#include "World/RainRunoff.h"
#include "World/Volcano.h"
#include "World/LavaCover.h"
#include "TriggerSettings.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

// IDLE_DEMO_V1: owns isolated weather engines. Live water/lava remain intact.
class IdleDemo {
public:
    float operatorGuiHeight()const{return gui&&visible?gui->getHeight():0;}
    void placeOperatorGui(float x,float y,float width){if(gui){if(gui->getWidth()!=width)gui->setWidth(width);gui->setPosition(x,y);}}

    void setup() {
        try {const auto path=ofToDataPath("idle-demo.json",true);if(ofFile::doesFileExist(path)) {
            auto j=ofLoadJson(path);enabled=j.value("enabled",true);demoSound=j.value("demo_sound",true);delay=ofClamp(j.value("idle_seconds",60.0f),30,300);
        }}catch(const std::exception&){enabled=true;delay=60;}
        rainHand.load("demo/rain-hand.png");moundHands.load("demo/mounding-hands.png");
        gui.reset(new ofxDatGui());gui->setAutoDraw(false);
        gui->addHeader("IDLE DEMO (J SHOW/HIDE)",false);
        toggle=gui->addToggle("Enable idle demo",enabled);
        gui->addToggle("Demo sound",demoSound);
        gui->addSlider("Idle delay sec",30,300,delay)->setPrecision(0);
        gui->addButton("Preview volcano demo");gui->addButton("Preview rain demo");
        status=gui->addLabel("Touch the sand to play");
        gui->onToggleEvent(this,&IdleDemo::onToggle);gui->onSliderEvent(this,&IdleDemo::onSlider);gui->onButtonEvent(this,&IdleDemo::onButton);
        lastActivity=ofGetElapsedTimef();
    }
    bool running()const{return idleSession;} // DEMO_LOOP_V2: includes the quiet gap
    void noteActivity(){lastActivity=ofGetElapsedTimef();requested=0;if(running())finish(false);}
    bool keyPressed(int key){if(key=='j'||key=='J'){visible=!visible;return true;}return false;}
    void update(const std::shared_ptr<KinectProjector>& k,bool allowed,bool playerBusy,ofVec2f rainAnchor) {
        float now=ofGetElapsedTimef();
        if(gui&&visible)gui->update();
        demoRain.setSoundEnabled(demoSound);demoVolcano.setSoundEnabled(demoSound);
        if(!enabled||!allowed||!k){if(running())finish(false);lastActivity=now;baseline.clear();requested=0;return;}
        bool moved=false;
        if(now>=nextMotion) {
            nextMotion=now+.20f;uint64_t serial=0;
            if(k->copyRainDepthSnapshot(raw,serial)&&raw.isAllocated()&&serial!=sequence) {
                sequence=serial;freshAt=now;
                auto r=k->getKinectROI();std::vector<float> heights;heights.reserve(24*18);
                int changed=0,raised=0;bool sameROI=haveROI&&r.x==motionROI.x&&r.y==motionROI.y&&r.width==motionROI.width&&r.height==motionROI.height;
                for(int y=0;y<18;++y)for(int x=0;x<24;++x) {
                    int px=int(r.x+(x+.5f)*r.width/24),py=int(r.y+(y+.5f)*r.height/18);float h=-10000;
                    if(px>=0&&py>=0&&px<int(raw.getWidth())&&py<int(raw.getHeight())) {
                        float d=raw[py*raw.getWidth()+px];if(d>=450&&d<=4500)h=k->rainElevationFromDepth(px,py,d);
                    }
                    if(!std::isfinite(h))h=-10000;
                    if(MagicSandTriggers::handHeight(h))++raised;
                    int i=int(heights.size());
                    if(sameROI&&baseline.size()==24*18&&h>-700&&baseline[i]>-700&&std::fabs(h-baseline[i])>12)++changed;
                    heights.push_back(h);
                }
                moved=raised>=2||changed>=4||(!sameROI&&haveROI);
                if(baseline.empty()||moved||!sameROI)baseline=heights;
                motionROI=r;haveROI=true;
            }
        }
        if(moved||now-freshAt>.60f) {
            lastActivity=now;
            if(running()){finish(false);std::cout<<"[IDLE DEMO] cancelled by interaction or unavailable depth"<<std::endl;}
            requested=0;return;
        }
        if(!idleSession&&playerBusy){lastActivity=now;requested=0;return;}
        if(phase==0&&(requested||(idleSession?now>=nextDemoAt:now-lastActivity>=delay))) {
            int kind=requested?requested:nextKind;requested=0;savedCover=MagicSandLavaCover::map();
            bool started=kind==1?demoVolcano.beginDemo(k):demoRain.beginDemo(k,rainAnchor.x,rainAnchor.y);
            if(!started){demoVolcano.endDemo();demoRain.endDemo();MagicSandLavaCover::map()=savedCover;lastActivity=now;nextDemoAt=now+5;return;}
            if(kind==2)MagicSandLavaCover::map().clear();
            idleSession=true;phase=kind;phaseAt=now;nextKind=kind==1?2:1;
            std::cout<<"[IDLE DEMO] "<<(kind==1?"volcano at current highest terrain":"rain hand demonstration")<<std::endl;
        }
        if(phase==1) {
            demoVolcano.update(k,true);
            if(now-phaseAt>=39)finish(true);
        } else if(phase==2) {
            demoRain.update(k,true);
            if(demoRain.demoFinished()||now-phaseAt>=50)finish(true);
        }
        if(status&&now>=nextLabel){nextLabel=now+.5f;status->setLabel(phase!=0?(phase==1?"Volcano demo: touch sand to play":"Rain demo: touch sand to play"):
            (idleSession?"Next demo in ":"Demo in ")+std::to_string(int(std::ceil(std::max(0.0f,idleSession?nextDemoAt-now:delay-(now-lastActivity)))))+" seconds");}
    }
    void drawWater(){if(phase==2)demoRain.draw();}
    void drawVolcano(){if(phase==1)demoVolcano.draw();}
    void drawTop() {
        if(phase==0)return;
        if(phase==2)demoRain.drawFlash();
        const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
        float age=ofGetElapsedTimef()-phaseAt;
        ofPushStyle();ofEnableAlphaBlending();ofSetColor(255);
        if(phase==1&&age<8) {
            auto p=demoVolcano.demoScreenPosition();float w=std::max(95.0f,std::min(250.0f,ofGetWidth()*.23f));
            float h=w*1.13f;float pulse=1+.035f*std::sin(age*2);
            moundHands.draw(p.x-w*.5f*pulse,p.y-h*.35f,w*pulse,h*pulse);
        } else if(phase==2&&age<6) {
            auto p=demoRain.cloudScreenPosition();float w=std::max(110.0f,std::min(270.0f,ofGetWidth()*.26f));
            float h=w*(891.0f/2048.0f);float bob=5*std::sin(age*2);
            rainHand.draw(p.x-w*.5f,p.y-h-15+bob,w,h);
        }
        // These are projected instructions for children, separate from controls.
        // BONE_BUTTON_V8: projected demo uses hand pictures without captions.

        ofPopStyle();
        if(depth)glEnable(GL_DEPTH_TEST);if(cull)glEnable(GL_CULL_FACE);if(stencil)glEnable(GL_STENCIL_TEST);
    }
    void drawGui(){if(gui&&visible){/* EDGE_GUI_V6: position assigned by main layout */gui->draw();}}
private:
    RainRunoff demoRain;Volcano demoVolcano;
    MagicSandLavaCover::Map savedCover;
    ofImage rainHand,moundHands;ofShortPixels raw;
    std::vector<float> baseline;ofRectangle motionROI;bool haveROI=false;
    uint64_t sequence=0;
    bool idleSession=false;float nextDemoAt=0;
    bool demoSound=true;bool enabled=true,visible=true;float delay=60,lastActivity=0,nextMotion=0,freshAt=-1000,phaseAt=0,nextLabel=0;
    int phase=0,nextKind=1,requested=0;
    std::unique_ptr<ofxDatGui> gui;ofxDatGuiLabel*status=nullptr;ofxDatGuiToggle*toggle=nullptr;
    void finish(bool completed){
        if(!idleSession)return;
        if(phase!=0){demoRain.endDemo();demoVolcano.endDemo();MagicSandLavaCover::map()=savedCover;}
        phase=0;float now=ofGetElapsedTimef();
        if(completed){nextDemoAt=now+12;std::cout<<"[IDLE DEMO] cycle complete; next demo in 12 seconds"<<std::endl;}
        else {idleSession=false;lastActivity=now;std::cout<<"[IDLE DEMO] session ended; normal play restored"<<std::endl;}
    }
    void save(){try{ofJson j;j["enabled"]=enabled;j["demo_sound"]=demoSound;j["idle_seconds"]=delay;ofSavePrettyJson(ofToDataPath("idle-demo.json",true),j);}catch(const std::exception&) {}}
    void onToggle(ofxDatGuiToggleEvent e){if(e.target->is("Demo sound")){demoSound=e.checked;demoRain.setSoundEnabled(demoSound);demoVolcano.setSoundEnabled(demoSound);}else{enabled=e.checked;noteActivity();}save();}
    void onSlider(ofxDatGuiSliderEvent e){if(!std::isfinite(e.value))return;delay=ofClamp(e.value,30,300);noteActivity();save();}
    void onButton(ofxDatGuiButtonEvent e){if(!enabled)return;noteActivity();requested=e.target->is("Preview volcano demo")?1:2;}
};
