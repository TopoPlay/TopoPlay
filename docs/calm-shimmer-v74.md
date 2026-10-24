# Calm-water shimmer V74

Adds a very faint moving white reflection to the V73 water surface. It reuses
the two already-computed slow breeze phases: no new texture, trigonometric calls,
geometry, water simulation steps, capture, or higher frame rate.

The added light is 0.012 + 0.010 sin(A) cos(B), where A and B are the existing
breeze phases. Its range is 0.002–0.022 before the existing water opacity mask
(about 0–4 RGB levels at normal sea opacity on an 8-bit output). It drifts over
tens of seconds. Thin rain has its existing lower opacity. The reflection stays
white and preserves the parent image's alpha, as in V73.

Only InteractiveWater.h and this new note are installed. Hand detection, wave
strength, shoreline capture/return, terrain blending, calibration, saved settings,
projector setup, draw-state handling, FPS and CPU limits are unchanged.

Original GPL notices and Evan Wallace's MIT ripple attribution remain intact.
This reflection-only addition does not change the license of other components.

Validation: shader compile/link and white-only/unchanged-alpha render checks in
Mesa EGL for legacy/modern and rectangle/normalized textures; installer exact-source
and restore guards. Full projector appearance still needs hardware confirmation.
