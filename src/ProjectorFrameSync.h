#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// TopoPlay modification: 2026-10-23. Synchronize the finished texture shared by
// the operator and projector contexts. No extra framebuffer or image readback.
#include "ofMain.h"
#include <cstdlib>
#include <iostream>
#include <string>

class TopoPlayProjectorFrameSync {
public:
    TopoPlayProjectorFrameSync()=default;
    TopoPlayProjectorFrameSync(const TopoPlayProjectorFrameSync&)=delete;
    TopoPlayProjectorFrameSync& operator=(const TopoPlayProjectorFrameSync&)=delete;
    // All calls occur on the application's one render thread with the named
    // window's GL context current. Sync objects belong to the shared group.
    void beforeWrite(){
        initialize();
        if(disabled)return;
        active=true;
        if(fences && consumed){glWaitSync(consumed,0,GL_TIMEOUT_IGNORED);glDeleteSync(consumed);consumed=nullptr;}
        if(fences && ready){glDeleteSync(ready);ready=nullptr;}
    }
    void afterWrite(){
        initialize();
        if(disabled)return;
        if(fences){ready=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);if(ready)glFlush();else glFinish();}
        else glFinish();
    }
    void beforeRead(){
        initialize();
        if(disabled)return;
        active=true;
        if(fences && ready)glWaitSync(ready,0,GL_TIMEOUT_IGNORED);
    }
    void afterRead(){
        initialize();
        if(disabled)return;
        if(fences){
            if(consumed)glDeleteSync(consumed);
            consumed=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            if(consumed)glFlush();else glFinish();
        } else glFinish();
    }
    // Called only in the operator context when leaving RUN. No work at all
    // before the first RUN frame or on subsequent Setup/calibration updates.
    void suspend(){
        if(!initialized || disabled || !active)return;
        if(fences && consumed){glWaitSync(consumed,0,GL_TIMEOUT_IGNORED);glDeleteSync(consumed);consumed=nullptr;}
        if(fences && ready){glDeleteSync(ready);ready=nullptr;}
        glFinish();active=false;
        std::cout<<"[PROJECTOR FRAME SYNC] suspended for Setup/calibration"<<std::endl;
    }
    void release(){
        if(released)return;
        initialize();
        if(disabled){glFinish();released=true;return;}
        if(fences && consumed){glWaitSync(consumed,0,GL_TIMEOUT_IGNORED);glDeleteSync(consumed);consumed=nullptr;}
        if(fences && ready){glDeleteSync(ready);ready=nullptr;}
        glFinish();active=false;released=true;
    }
    // Do not invoke GL in the destructor: OF may destroy windows before the
    // application object. At most two remaining fences are freed with the group.
private:
    bool initialized=false,fences=false,released=false,disabled=false,active=false;
    GLsync ready=nullptr,consumed=nullptr;
    void initialize(){
        if(initialized)return;
        initialized=true;
        const char* mode=std::getenv("TOPOPLAY_FRAME_SYNC");
        disabled=mode && std::string(mode)=="off";
        if(disabled){std::cout<<"[PROJECTOR FRAME SYNC] disabled for comparison"<<std::endl;return;}
        const bool forceFinish=mode && std::string(mode)=="finish";
        fences=!forceFinish && (GLEW_VERSION_3_2 || GLEW_ARB_sync) && glFenceSync && glWaitSync && glDeleteSync;
        std::cout<<"[PROJECTOR FRAME SYNC] "<<(fences?"shared GPU fences":"completion fallback")<<std::endl;
    }
};
