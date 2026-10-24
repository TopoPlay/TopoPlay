#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ofMain.h"
#include <iostream>

// openFrameworks supports independent window lifetimes. Closing only the
// operator window otherwise leaves the projection window/main loop alive.
// Listen only for operator EXIT, never visibility, iconify or monitor changes:
// those changes are expected when the laptop lid is closed.
class TopoPlayShutdown {
public:
    void operatorClosed(ofEventArgs&) {
        if (requested) return; // The loop may deliver exit again during cleanup.
        requested = true;
        std::cout << "[TOPOPLAY SHUTDOWN V80] Operator window closed; exiting all windows."
                  << std::endl;
        ofExit(0); // Normal framework cleanup; no exit(), abort() or kill().
    }
private:
    bool requested = false;
};
