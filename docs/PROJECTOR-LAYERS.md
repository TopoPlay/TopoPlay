# Projector scene layers and shared-frame handoff

Operator-tested update, 2026-10-23. On the tested Dell Inspiron 5570 setup,
no projector flicker was observed after applying the scene layers and shared-frame
handoff changes with water ripples enabled. Intermittent issues may still require
longer testing; this result does not establish behavior on other hardware.

Back-to-front composition is terrain and pool base, existing map overlay, animals,
additive white ripples, lava/volcano effects, trees/fire, and the existing overhead
cloud/flash pass. Controls and the final gesture ceiling mask retain their existing
roles. Demo water is split into the same base and ripple passes. No simulation,
wave strength, gestures, calibration, CPU limit or frame-rate setting is changed.

The tree layer reuses its existing cached meshes and is omitted from splash/result
screens as before. No extra full-resolution framebuffer, camera capture, readback,
texture or simulation pass is added. Ripple lighting stays at its existing size.

The operator context produces projectorComposite; the projector context samples
its texture. A producer fence plus flush publishes each completed composite. A
consumer fence plus flush prevents the next write from racing a projector read.
Waits are queued on the GPU. Systems lacking sync support use glFinish at the
producer/consumer boundaries. For diagnosis only, TOPOPLAY_FRAME_SYNC=finish
forces this fallback. The operator confirmed acceptable appearance during the test session.

References:
- https://wikis.khronos.org/opengl/Sync_Object
- https://wikis.khronos.org/opengl/GLAPI/glFenceSync

Keep original GPL and component MIT notices. The new synchronization helper is
GPL-2.0-or-later. The original credited thunder and all runtime media are unchanged.
