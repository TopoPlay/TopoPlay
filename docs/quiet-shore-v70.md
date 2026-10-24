# Quiet shoreline — V70

V70 strongly absorbs returning shoreline motion while retaining the incoming
10x hand-push/pressure boost, actual water storage, run-up, and pullback.
A graded 24-cell bank band reduces stored discharge when it points away
from the nearest raised bank. Gravitational pressure still draws water down
and out of a wet bank; the new term removes momentum, not water. It is a
project-specific teaching approximation rather than an exact non-reflecting
boundary condition, and suppression varies with basin shape and depth.

The nearest-bank search is bounded and reused across the transport passes.
For regular sea it tracks dry land at/above the configured waterline. For
rain ponds, a raised dry bank qualifies, while a dry low breach or an advancing
wetting front is excluded. This preserves the stronger release through a dam.

The decorative ripple solver also has a graded 16-cell absorbing edge band,
damping both height and velocity so the fine ripples do not repeatedly echo.
Near shore, fine ripple detail is intentionally calmer. The physical surge
and shoreline movement continue to use the conservative water solver.

V67 global settling, V68's 10x boost, thin rain, rain amount, pond evaporation,
GUI, terrain levels, calibration/ROI and V69's MIT/source credits are retained.
No display/blending fixes are included: check the cloud/tree flicker after
this update, independently of how quiet the waves look. There is no larger
render target, higher FPS, added camera capture, or increased substep cap.
The shore-map calculation adds a small CPU operation per hydraulic tick;
actual laptop temperatures still need checking at 15 FPS.

Validation: the isolated plane-wave test reduced returning momentum energy
to about 1.32% of V69 (roughly 99% lower) over its measured echo window. The
water still ran up the bank and pulled back; closed-basin mass was conserved
within float rounding. Late visual-ripple energy in the reflected region
fell to about 2.81% of V69. These are fixture-specific results, not a promise
of zero reflection for every terrain. The initial localized run-up remained
about 19 mm and the downstream crest from a 30-mm pond remained larger than
from an 8-mm pond. Rain counted once, lake-at-rest, positive depths, changing
terrain, narrow-grid safety and the eight-pass cap pass. Integrated 15 FPS
hand tracking, rest/push/restart/toggle tests pass. Eight real GLSL paths
compile/link/animate, and quarter-resolution alpha/compositing checks pass.

Only RainWaterGrid.h and WaterRippleGrid.h executable source change. Existing
Evan Wallace copyright/MIT notices and all GPL/source-provenance comments
are retained. See WATER-MOVEMENT.md and ../licenses/evan-water-MIT.txt.
