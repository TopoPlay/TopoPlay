#pragma once
#include "ofMain.h"
#include "KinectProjector/KinectProjector.h"
#include <iostream>
#include <memory>
// AUTO_RUN_V32: a one-shot startup request, never a permanent Run enforcer.
class MagicSandAutoRun {
public:
    void begin(bool savedCalibration){
        pending=savedCalibration;
        std::cout<<"[AUTO RUN V32] "<<(pending?"Saved calibration found; waiting for Kinect depth to stabilize.":
            "No saved calibration; keeping Setup.")<<std::endl;
    }
    void update(const std::shared_ptr<KinectProjector>& projector){
        if(!pending||!projector)return;
        // Manual Run or entering calibration takes priority over autostart.
        if(projector->GetApplicationState()!=KinectProjector::APPLICATION_STATE_SETUP){pending=false;return;}
        if(!projector->isImageStabilized())return;
        pending=false;
        // Preserve the native validation/loading path and all its notifications.
        projector->startApplication();
        std::cout<<"[AUTO RUN V32] "<<(projector->GetApplicationState()==KinectProjector::APPLICATION_STATE_RUNNING?
            "Started RUN using saved calibration.":"Saved calibration/settings could not start RUN; keeping Setup. Check the preceding KinectProjector messages.")<<std::endl;
    }
private:
    // Consume the request once: returning to Setup must stay under operator
    // control rather than automatically forcing RUN again. Readiness is the
    // native filter signal; it does not verify geometric calibration accuracy.
    bool pending=false;
};
