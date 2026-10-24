# Steady terrain V78

## Observed cadence
In the uploaded code SandSurfaceRenderer redraws once per 15 application frames,
which is 1 Hz at the user's 15 FPS. TerrainHabitatMap also refreshes once per
second; pine geometry is cached for 0.5 seconds. These scheduled updates can
expose filtered depth jitter as small steps. The old renderer comment saying
3.75 FPS was stale; the actual existing modulo-15 budget is retained.

## Changes
- A material-height cache reads the already available FILTERED CPU depth image
  only when the terrain was already due for redraw. It ignores deviations up to
  2 mm. Sustained 2–6 mm changes are confirmed across samples at least 0.24 seconds
  apart. Changes of 6 mm or more are accepted at the existing redraw immediately.
  Small real edits can therefore take one additional normal refresh to appear.
- Cached physical height is stored in terrain mesh Z. BOTH vertex shaders use
  it for material/color elevation only. They still overwrite their local geometry
  Z with the existing live filtered depth before calibrated projection. Invalid
  initial readings use a sentinel and fall back to the original shader elevation.
- ROI, base-plane and conversion changes clear the material cache. GUI mapping
  edits still take effect through the existing look-dirty/redraw path.
- Habitat physical elevation holds changes within 2 mm before its existing EMA.
  Material remapping still uses the current terrain profile.
- Pine projected feet hold within 1.5 projector pixels, and projected radii within
  0.35 pixels. Larger changes are accepted on the existing mesh refresh; uphill
  selection and fire/burn logic are preserved.

No camera capture frequency, averaging, raw-depth handling, ceiling settings,
wave-hand protection, rain sampling, water physics, ambient ripples, frame rate,
CPU cap, calibration/ROI file, saved terrain levels, or projector launcher is
changed. V77 is not a dependency and is not installed/removed by this package.
The cache adds one bounded CPU pass over the existing terrain vertices during
scheduled redraws, and uploads vertices only when accepted material Z changes.
No new camera capture, GPU readback, image filter pass or full-screen buffer is
added. Actual temperature/performance still needs checking on the user's system.

This targets the slight terrain/tree updates visible on BOTH screens. It does
not claim to diagnose or cure the separate intermittent projector-only blinking.

## Validation
- Standalone hysteresis tests: 600 quiet samples remain unchanged; larger edits
  immediate; sustained small edits accepted; transient/invalid values and reset.
- Actual extracted renderer method compiled/exercised with a minimal API mock:
  only filtered data used, held heights stable, larger change accepted, ceiling
  rejection and cache reset work. This is not a full openFrameworks build.
- Actual pine-mesh method compiled/exercised with API mocks: sub-pixel foot/radius
  jitter held, larger projection change accepted, valid mesh generated.
- Actual GL2/GL3 vertex shaders compiled/linked with the installed fragment shaders
  under Mesa EGL. Render tests verified held material heights and pixel-identical
  projection geometry across live-depth changes, plus the invalid sentinel path.
- Installer exact-source checks, unchanged water/settings checks, idempotence,
  backup validation, later-edit guard and exact restore were tested separately.

Original Magic-Sand/AR Sandbox GPL attribution remains in the existing files.
The new cache helper is GPL-2.0-or-later. Water code and its existing Evan Wallace
MIT attribution are untouched.
