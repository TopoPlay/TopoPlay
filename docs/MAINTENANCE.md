# Maintenance notes

The inline comments explain decisions near their implementation. These notes
connect the systems without turning version tags into a second specification.

- **Startup:** `AutoRunStartup.h` consumes one request after the native stable-depth
  signal. It calls native RUN so the normal calibration loader remains authoritative.
  RUN and saved-profile restoration can reset camera filter buffers again. Do not
  treat the first ready frame as permission to cache button placement permanently.
  The readiness check does not prove the loaded calibration maps onto the projector.
- **Saved operator profile:** `ofApp::updateBoxSettings` restores the profile once,
  after RUN, because native RUN resets sea level. Continuous restoration would undo
  live GUI changes. Native camera XML and the JSON profile contain overlapping
  settings; keep that order explicit when making future changes. The earlier
  black-screen incident was resolved by recalibration; its original cause was not
  established and is not attributed to the GUI by this cleanup.
- **Coordinates:** camera pixels, projector pixels, depth/world Z and elevation
  above the base plane are distinct. Calibration maps points in three dimensions;
  moving an icon in projector pixels alone does not move its sensing center.
  AutoBonePlacement shares the selected candidate between drawing and detection;
  Dig uses independent state at the opposite corner.
- **Small rings:** FlexibleRing deliberately checks only one center pixel and
  searches a dense radial band. Broad center averages and sparse radius probes
  previously made small holes and thin fingers unreliable. Gesture heights use
  raw depth rather than terrain filtering. A sensor/projector viewing-angle
  difference can still occlude a raised ring near an edge; calibration cannot
  restore depth samples the sensor cannot see.
- **ROI containment:** BoneRoiFootprint inversely projects the icon outline at
  candidate depth, not just its center. FossilRoiSurface projects camera ROI mesh
  samples; a full-window image would cover beyond the regular terrain footprint.
- **GUI lockouts:** mode visibility flags also lock out activation and exit the
  corresponding active mode. Poll the GUI once per application update, even when
  the normal animal simulation is skipped. Historical mother-animal internals
  and legacy game paths remain: removing executable systems needs a separate
  functional review, beyond this comments-only cleanup.
- **Water:** RainWaterGrid uses conservative friction-dominated surface flow.
  Surface level is terrain elevation plus local water depth. It omits momentum
  waves. Long pool persistence affects evaporation; drainage still responds to
  changed terrain. RainRunoff refreshes terrain from raw samples because a narrow
  dam breach may remain smoothed over in the terrain filter. Pool shimmer changes
  appearance independently of water transport. Its animated highlight mesh is not
  a second fluid solver.
- **Volcano:** retain the existing source/latch/reset logic and local summit checks.
  Smoke, tree fire and texture animation have separate budgets. LavaSurfaceMotion
  only bends texture coordinates, preserving heat/alpha and simulation behavior.
- **Performance:** a CPU frequency percentage is not a temperature thermostat.
  Frame rate independently limits recurring work; 15–20 FPS was useful on the
  tested Dell. Mesh caches, reduced refresh rates and acquisition-thread queues
  are intentional. Avoid adding synchronous camera readbacks to draw methods.
- **Compatibility:** GLSL 120/150 and rectangle/normalized texture paths serve
  different renderer configurations. Keep fallback draws. Versioned log labels
  remain useful for diagnosing installed versions; they are not a request to
  reinstate old patches. Future installers should use this cleanup as a new
  baseline because comments and file hashes have changed.

Original copyright/license headers and bundled dependency files are retained.
The original README supplies the upstream project attribution; this source-only
cleanup does not establish redistribution rights for runtime sounds or images.
