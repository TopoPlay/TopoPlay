# Background ripples

Historical development note: this records an individual experiment or patch.
Later updates may supersede its behavior and validation results. Its filename
is retained for existing source references.

Replaces the earlier patterned ambient white-light term. Instead, occasional low
strength packets are injected into the existing WaterRippleGrid used by hands.
They spread, interact with boundaries and decay through the existing ripple code.
Their visible highlights use the existing white-only reflection shader.

A packet is attempted about every 1.8–3.8 seconds, with varied position, direction,
radius and strength. It prefers a few wet cells away from a shore/ROI edge, and
falls back to enclosed pool interiors. Very thin rain and isolated single wet
cells are excluded. The source starts after one second and resets with the ROI.
No patterned overlay, extra texture, hydraulic force, or water volume is added.

Hand gestures, stronger pushes, shoreline storage/drainage, terrain blending,
calibration, settings, FPS, draw-state/alpha isolation and projector launcher are
unchanged. At most forty cheap CPU candidate checks are made per ambient event;
wave stepping, mesh size and lighting resolution are unchanged.

Keep original project GPL notices and Evan Wallace's MIT ripple attribution and
license file. This wrapper uses the existing adapted ripple component; it does
not change other component licenses.

Validation: standalone actual ripple/helper tests for bounded strength, varied
source positions, wet-mask respect and decay; integration header compile with API
mock; GL2/GL3 and both texture-target reflection shader compile/render tests;
installer/restore/source guards. Projector visibility needs a hardware test.
