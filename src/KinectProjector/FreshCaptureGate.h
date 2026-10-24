#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// TopoPlay calibration capture lifecycle, 2026-10-24.
#include <atomic>
class TopoPlayFreshCaptureGate {
public:
    void colorReceived(){++colorSequence;}
    void patternChanged(){lastCounted=colorSequence;freshFrames=0;}
    void resetRequested(){pending.store(true,std::memory_order_release);patternChanged();}
    void resetComplete(){pending.store(false,std::memory_order_release);}
    bool waitingForReset() const{return pending.load(std::memory_order_acquire);}
    bool ready(bool depthStable,int required){
        if(waitingForReset()||!depthStable)return false;
        if(lastCounted==colorSequence)return false;
        lastCounted=colorSequence;
        if(++freshFrames<required)return false;
        freshFrames=0;return true;
    }
private:
    std::atomic<bool> pending{false};
    unsigned long long colorSequence=0,lastCounted=0;
    int freshFrames=0;
};
