# Responsive operator-window shutdown

Operator-tested update 2026-10-23. The operator reported about three seconds
to close, then confirmed repeated use was satisfactory. This is a result on the
tested setup, not a guaranteed shutdown time.

 Clicking the operator X requests shutdown without immediately
destroying either GLFW context. The main loop stays alive to service desktop
messages while the Kinect worker finishes its bounded frame wait and stops/closes
the USB device on its owning thread. No concurrent UI-thread device close or
unsafe detached camera worker is introduced.

During this phase, terrain/water/animals, GUI interaction and new shared-frame
work stop. The projector is cleared and the operator window shows a closing
message. When the worker's separate atomic completion flag is set, pending shared
frame work is drained with the operator GL context still alive, settings save
once through the normal exit path, and the framework closes the application.

The change does not force-kill a stuck USB driver, detach an active worker or
promise a fixed close time. If device stop/close still takes a long time, the
window remains responsive and logs identify that exact stage every two seconds.
Capture the terminal output through shutdown for diagnosis. No runtime frame-rate,
CPU, sound asset, gesture or calibration setting is changed by this patch.

References: openFrameworks 0.12.1 ofMainLoop.cpp, ofAppGLFWWindow.cpp, ofThread.cpp.
Their normal close path and thread completion semantics informed this change.
The tested projector layer/synchronization work is retained.
