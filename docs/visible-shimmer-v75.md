# Visible background shimmer V75

Changes only the ambient white reflection contribution from V74 by four times:
0.048 + 0.040 sin(A) cos(B), using the exact same existing slow breeze phases.
The range is 0.008–0.088 before the existing water opacity mask (about 1–15 RGB
levels at regular sea opacity). No extra trigonometric calls, textures, geometry,
physics steps, capture, frame rate or simulation energy are added.

V73/V74's white-only additive blend, unchanged parent alpha, display smoothing,
hand sweeps, hydraulic momentum, shore storage/return, terrain softness, settings,
calibration, projector launcher and CPU limits remain unchanged.

Requires installed V74. The installer makes and verifies a source backup. Restore
returns the exact previous V74 renderer. Shader compile/render checks cover both
GL versions and texture targets, white light only, preserved alpha, and a stronger
but equally slow still-water animation. Actual visibility needs a projector check.

Existing GPL notices and Evan Wallace MIT ripple attribution are retained.
