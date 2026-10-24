/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
/***********************************************************************
Main.cpp
Copyright (c) 2016-2017 Thomas Wolf and Rasmus R. Paulsen (people.compute.dtu.dk/rapa)

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
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/


#include "ofMain.h"
#include "ofApp.h"
#include <GLFW/glfw3.h>
#include "ProjectorDisplayGuard.h"
#include "TopoPlayShutdown.h"

const std::string TopoPlayVersion = "1.0.1"; // Based on the operator-tested V78 build.

bool setWindowDimensions(ofGLFWWindowSettings& settings, int windowsNum)
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);

    std::cout << "Number of screens found: " << count << std::endl;

    if (monitors != nullptr && count > windowsNum)
    {
        int xM = 0;
        int yM = 0;

        glfwGetMonitorPos(monitors[windowsNum], &xM, &yM);

        const GLFWvidmode* desktopMode =
            glfwGetVideoMode(monitors[windowsNum]);

        if (desktopMode == nullptr)
        {
            return false;
        }

        std::cout << "Monitor " << windowsNum
                  << " size: "
                  << desktopMode->width
                  << "x"
                  << desktopMode->height
                  << std::endl;

        if (windowsNum == 0)
        {
            settings.setSize(
                static_cast<int>(desktopMode->width * 0.8),
                static_cast<int>(desktopMode->height * 0.8)
            );
        }
        else
        {
            settings.setSize(
                desktopMode->width,
                desktopMode->height
            );
        }

        settings.setPosition(glm::vec2(xM, yM));

        return true;
    }

    settings.setSize(1600, 800);
    settings.setPosition(glm::vec2(0, 0));

    return false;
}


//========================================================================
int main()
{
    ofGLFWWindowSettings settings;

    // Create the first window so GLFW is initialized
    settings.setSize(1600, 800);
    settings.setPosition(glm::vec2(0, 0));
    settings.resizable = true;
    settings.decorated = true;
    settings.title = "TopoPlay " + TopoPlayVersion;

    std::shared_ptr<ofAppBaseWindow> mainWindow =
        ofCreateWindow(settings);

    // Resize/reposition main control window
    setWindowDimensions(settings, 0);

    mainWindow->setWindowPosition(
        ofGetScreenWidth() / 2 - settings.getWidth() / 2,
        ofGetScreenHeight() / 2 - settings.getHeight() / 2
    );

    mainWindow->setWindowShape(
        settings.getWidth(),
        settings.getHeight()
    );

    // Create projector window on second display
    setWindowDimensions(settings, 1);

    settings.title = "TopoPlay Projector";
    settings.resizable = false;
    settings.decorated = false;
    settings.shareContextWith = mainWindow;

    std::shared_ptr<ofAppBaseWindow> secondWindow =
        ofCreateWindow(settings);

    secondWindow->setVerticalSync(false);

    std::shared_ptr<ofApp> mainApp(new ofApp);

    ofAddListener(
        secondWindow->events().draw,
        mainApp.get(),
        &ofApp::drawProjWindow
    );

    mainApp->projWindow = secondWindow;

    // PROJECTOR_FOLLOW_V49: monitor identities survive primary-screen changes.
    // Keep the guard alive for the full loop and poll without changing app pacing.
    MagicSandProjectorDisplayGuard projectorDisplay(secondWindow);
    ofAddListener(mainWindow->events().update,&projectorDisplay,&MagicSandProjectorDisplayGuard::update);
    // TOPOPLAY_CLOSE_V80: the operator owns the lifetime of both windows.
    // A lid close/minimize is not an exit event, so projector following remains active.
    TopoPlayShutdown shutdown;
    ofAddListener(mainWindow->events().exit, &shutdown,
                  &TopoPlayShutdown::operatorClosed, OF_EVENT_ORDER_BEFORE_APP);
    ofRunApp(mainWindow, mainApp);
    ofRunMainLoop();
    ofRemoveListener(mainWindow->events().exit, &shutdown,
                     &TopoPlayShutdown::operatorClosed, OF_EVENT_ORDER_BEFORE_APP);
    ofRemoveListener(mainWindow->events().update,&projectorDisplay,&MagicSandProjectorDisplayGuard::update);
}
