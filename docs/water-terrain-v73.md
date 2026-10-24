# Water and terrain update

Historical development note: this records an individual experiment or patch.
Later updates may supersede its behavior and validation results. Its filename
is retained for existing source references.

This patch is based on the user's current-fixes source snapshot. It does not
write calibration, ROI, saved terrain heights, map XML, CPU limits, or FPS.

## Reflections and flicker
The ripple layer adds white reflected light to the existing mapped water colors;
it has no blue artwork or black replacement-water shading. It preserves parent
alpha and explicitly sets additive blending, isolating the layer from inherited
alpha-test/logic-op state in the legacy GL path. A 0.12-second display-only filter
reduces normal-map/specular jitter. Physics and hand input are not filtered.
The existing quarter-pixel lighting buffer (at most 640x360), wave grid limit,
and projector/lid launcher are retained. No cross-context synchronization changes
are included. The projector-only flicker must still be checked on the user's
hardware; passing software GL tests cannot prove that it is eliminated.

## Hand sweeps
WaterSweep accumulates displacement across samples, rather than requiring one
large movement in every frame. Its minimum displacement is 6 mm and 0.3 wave-grid
cells, minimum speed 45 mm/s, cooldown 0.20 seconds. Similar, nearby samples can
span a brief depth dropout. A stationary hand cannot inject repeated waves.
The low-water gesture accepts raw clearance about 8–200 mm above the edited
waterline (full weight 22–160 mm), independent of the ceiling-height ring/rain
controls. Try a flat hand roughly 25–100 mm above water and sweep sideways.
Larger/faster sweeps produce a stronger hydraulic push and a visible surface wake.

## Shore capture and return
The hand-only momentum input is stronger; gravity/pressure acceleration is NOT
multiplied. Shore faces convert incident kinetic head into conservative water
storage on the higher bed (bounded visual run-up head). Land reached by run-up
uses overdamped, head-driven flow; unrelated rain ponds retain inertial flow.
After an impact, a narrow directional hydraulic shore zone damps RETURNING
momentum only, and fresh hand input has a short override, so
incoming pushes are not globally weakened. Visual ripple absorption is confined
to cells directly touching a dry bank, with original interior ripple dynamics.
The raised water drains downhill and retains existing evaporation rules. Lowering
terrain releases its run-up flag; no fixed old shoreline or water elevation is
hardcoded. This is a sandbox effect, not an engineering flood predictor.

## Terrain transitions
New GUI control: Transition softness % (default 40). Save box settings persists it.
0 restores the original mapping/sharpness; 100 gives a 16 mm half-width.
The shader averages the existing monotone material mapping over a physical-height
window, widening compressed transitions without changing saved layer heights.
Water color bands also get a small height-based color blend. Basic untextured
color maps retain their previous rendering. Physical elevations, animal habitats,
water storage, contours, and gestures do not change when softness is adjusted.

## Attribution
Keep the original Magic-Sand and AR Sandbox GPL notices and project credits.
WaterRippleGrid.h retains Evan Wallace's MIT attribution and requires the existing
licenses/evan-water-MIT.txt on distribution. This patch does not relicense the
whole program as MIT. RainWaterGrid.h remains project-specific GPL code with its
existing numerical-method references.

## Validation and limits
Standalone C++ numerical tests cover cumulative/strong/stationary hand sweeps,
conservation, wet/dry positivity, bounded substeps, raised-shore storage/drainage,
dam opening, and rainfall counting. An isolated shore-pulse test measured about
92% less returning-discharge energy over its selected observation window than the
uploaded solver. This is a synthetic comparison, not a universal reduction claim.
Actual header integration was compiled with a minimal API mock and exercised.
Both terrain fragment shaders and all four reflection shader variants were
compiled/linked with Mesa EGL. Render checks verified additive white light with
unchanged parent alpha, and softer transitions with the uploaded narrow bands.
A full openFrameworks/Kinect build and projector/temperature tests require the
user's machine. The installer runs make -j2 and launches only on build success.
