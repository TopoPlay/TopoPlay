#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// TopoPlay shutdown update 2026-10-23: defer framework/window teardown until the
// Kinect worker has finished. Keep OS events responsive throughout that wait.
#include "ofMain.h"
#include "ofAppGLFWWindow.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <iostream>
#include <stdexcept>

class TopoPlayShutdown {
public:
    void attach(const std::shared_ptr<ofAppBaseWindow>& operatorWindow,
                std::function<void()> begin,std::function<bool()> finished,
                std::function<void()> releaseGraphics){
        window=std::dynamic_pointer_cast<ofAppGLFWWindow>(operatorWindow);
        if(!window||!window->getGLFWWindow())throw std::runtime_error("TopoPlay requires a live GLFW operator window for orderly shutdown.");
        if(active())throw std::runtime_error("TopoPlay shutdown handler already attached.");
        beginStop=std::move(begin);workerFinished=std::move(finished);finishGraphics=std::move(releaseGraphics);
        active()=this;native=window->getGLFWWindow();
        previous=glfwSetWindowCloseCallback(native,&TopoPlayShutdown::closeRequested);
        ofAddListener(window->events().update,this,&TopoPlayShutdown::update,OF_EVENT_ORDER_BEFORE_APP);
    }
    void detach(){
        if(window){
            ofRemoveListener(window->events().update,this,&TopoPlayShutdown::update,OF_EVENT_ORDER_BEFORE_APP);
            if(native&&window->getGLFWWindow()==native)glfwSetWindowCloseCallback(native,previous);
        }
        if(active()==this)active()=nullptr;
        native=nullptr;window.reset();beginStop={};workerFinished={};finishGraphics={};
    }
    ~TopoPlayShutdown(){detach();}
    void update(ofEventArgs&){
        if(!requested||exiting)return;
        if(!started){started=true;startedAt=ofGetElapsedTimef();nextReport=startedAt+2;
            std::cout<<"[SHUTDOWN] Stop requested; keeping windows responsive."<<std::endl;
            beginStop();
        }
        if(!workerFinished()){
            const float now=ofGetElapsedTimef();
            if(now>=nextReport){nextReport=now+2;std::cout<<"[SHUTDOWN] Waiting for Kinect cleanup: "<<now-startedAt<<" seconds"<<std::endl;}
            return;
        }
        std::cout<<"[SHUTDOWN] Kinect finished after "<<ofGetElapsedTimef()-startedAt<<" seconds; releasing graphics."<<std::endl;
        finishGraphics();exiting=true;
        std::cout<<"[SHUTDOWN] Graphics released; closing all windows."<<std::endl;
        ofExit(0);
    }
private:
    std::shared_ptr<ofAppGLFWWindow> window;
    GLFWwindow* native=nullptr;GLFWwindowclosefun previous=nullptr;
    std::function<void()> beginStop,finishGraphics;std::function<bool()> workerFinished;
    bool requested=false,started=false,exiting=false;float startedAt=0,nextReport=0;
    static TopoPlayShutdown*& active(){static TopoPlayShutdown* handler=nullptr;return handler;}
    static void closeRequested(GLFWwindow* target){
        auto* self=active();if(!self||target!=self->native)return;
        // Suppress GLFW's immediate close. OF's exit notification is deliberately
        // delayed until acquisition has stopped and GPU use is drained.
        glfwSetWindowShouldClose(target,GLFW_FALSE);self->requested=true;
    }
};
