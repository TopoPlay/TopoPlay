/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Educational fork: keep the dedicated projection window on its physical display.
// Desktop compositors may move a borderless window when the laptop panel closes.
// Query both the monitor layout AND the actual window geometry: a compositor can
// move the window even when the reported desktop layout returns to the same shape.
#include "ofMain.h"
#include "ofAppGLFWWindow.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <string>

class MagicSandProjectorDisplayGuard {
public:
    explicit MagicSandProjectorDisplayGuard(const std::shared_ptr<ofAppBaseWindow>& window)
    : projector(window) { selectInitialTarget(); }

    void update(ofEventArgs&) {
        const double now=glfwGetTime();
        if(now<nextCheck)return;
        nextCheck=now+0.5; // Native window queries only; no terrain/depth work.
        auto window=std::dynamic_pointer_cast<ofAppGLFWWindow>(projector);
        if(!window)return;
        GLFWwindow* native=window->getGLFWWindow();
        int count=0;GLFWmonitor** monitors=glfwGetMonitors(&count);
        if(!native || !monitors || count<1)return;
        GLFWmonitor* found=nullptr;
        for(int n=0;n<count;++n)if(monitors[n]==target){found=monitors[n];break;}
        if(!found)for(int n=0;n<count;++n){
            const char* name=glfwGetMonitorName(monitors[n]);
            if(name && targetName==name){found=monitors[n];break;}
        }
        // Wait for the selected projector to reconnect; do not silently select
        // the laptop panel just because the projector was unplugged.
        if(!found)return;
        target=found;
        const GLFWvidmode* mode=glfwGetVideoMode(found);
        if(!mode || mode->width<=0 || mode->height<=0)return;
        int x=0,y=0,wx=0,wy=0,ww=0,wh=0;
        glfwGetMonitorPos(found,&x,&y);
        glfwGetWindowPos(native,&wx,&wy);glfwGetWindowSize(native,&ww,&wh);
        const bool moved=wx!=x || wy!=y;
        const bool resized=ww!=mode->width || wh!=mode->height;
        const bool minimized=glfwGetWindowAttrib(native,GLFW_ICONIFIED)==GLFW_TRUE;
        const bool hidden=glfwGetWindowAttrib(native,GLFW_VISIBLE)==GLFW_FALSE;
        const bool layoutChanged=count!=lastCount || x!=lastX || y!=lastY || mode->width!=lastWidth || mode->height!=lastHeight;
        // Keep desktop/operator windows from covering the dedicated projection.
        if(glfwGetWindowAttrib(native,GLFW_FLOATING)==GLFW_FALSE)
            glfwSetWindowAttrib(native,GLFW_FLOATING,GLFW_TRUE);
        if(minimized)glfwRestoreWindow(native);
        if(hidden)glfwShowWindow(native);
        if(resized)glfwSetWindowSize(native,mode->width,mode->height);
        if(moved)glfwSetWindowPos(native,x,y);
        // Focus only for a repair with a single active display. While the lid is
        // open, typing and calibration controls retain operator-window focus.
        if(count==1 && (moved || minimized || hidden || layoutChanged))glfwFocusWindow(native);
        if(layoutChanged || moved || resized || minimized || hidden){
            std::cout<<"[PROJECTOR FOLLOW V49] monitor="<<targetName<<" screens="<<count
                     <<" position="<<x<<","<<y<<" size="<<mode->width<<"x"<<mode->height
                     <<" repaired="<<(moved || resized || minimized || hidden)<<std::endl;
        }
        lastCount=count;lastX=x;lastY=y;lastWidth=mode->width;lastHeight=mode->height;
    }
private:
    std::shared_ptr<ofAppBaseWindow> projector;
    GLFWmonitor* target=nullptr;
    std::string targetName;
    double nextCheck=0;
    int lastCount=-1,lastX=0,lastY=0,lastWidth=0,lastHeight=0;
    void selectInitialTarget(){
        auto window=std::dynamic_pointer_cast<ofAppGLFWWindow>(projector);
        if(!window)return;
        GLFWwindow* native=window->getGLFWWindow();
        int count=0;GLFWmonitor** monitors=glfwGetMonitors(&count);
        if(!native || !monitors || count<1)return;
        int x=0,y=0,w=0,h=0;
        glfwGetWindowPos(native,&x,&y);glfwGetWindowSize(native,&w,&h);
        long long best=-1;
        for(int n=0;n<count;++n){
            int mx=0,my=0;glfwGetMonitorPos(monitors[n],&mx,&my);
            const GLFWvidmode* mode=glfwGetVideoMode(monitors[n]);if(!mode)continue;
            long long area=static_cast<long long>(std::max(0,std::min(x+w,mx+mode->width)-std::max(x,mx))) *
                           std::max(0,std::min(y+h,my+mode->height)-std::max(y,my));
            if(area>best){best=area;target=monitors[n];}
        }
        if(target){const char* name=glfwGetMonitorName(target);targetName=name?name:"";}
    }
};
