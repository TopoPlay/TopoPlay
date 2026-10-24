/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
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
