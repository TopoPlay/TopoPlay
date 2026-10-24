# Wave behavior restoration

Historical development note: this records an individual experiment or patch.
Later updates may supersede its behavior and validation results. Its filename
is retained for existing source references.

This restoration returns to the earlier RainWaterGrid dynamics, including its 3.5-second
momentum settling term. It restores the pre-absorption WaterRippleGrid dynamics and
the earlier normal-map display gains (2.5 for slopes and 0.75 for height). The experimental
10x physical forcing and broad physical/visual shore absorption are removed.
This intentionally restores the original shoreline reflection response too.

The broad visual shore absorber was affecting most of small pools, not just
their last edge pixels. In the test fixture, a hand-footprint ripple retained
energy 0.147118 with the earlier solver versus 0.000310101 with broad shore absorption after 50 small steps.
That is a concrete explanation for losing the waterlike hand-motion surface;
it was not necessary to keep changing the operator's hand height or terrain.

The mapped-color transparent highlights/shadows and scoped drawing-state
cleanup remain byte-for-byte. It still requires no separate water artwork,
fixed blue sea overlay or larger render target. The uploaded shifted-waterline
reference, newest CPU snapshot sampling and brief moving-hand terrain guard
remain. RainRunoff.cpp is not changed by this patch. Terrain maps/levels,
calibration/ROI, GUI, frame rate, assets and MIT/project credits are untouched.
Rain volume, pooling, dam-break surges and wet/dry run-up remain available;
they now use the earlier strength/response rather than the 10x experiment.

Tests compare the actual restored hydraulic arrays against the earlier reference solver after
every timestep, across hand pushes, a changed bank, wet/dry movement and rain:
water and both flow arrays match exactly. Surface displacement and velocity
also match the pre-10x ripple solver exactly. The visual ripple restoration
is tested against the broad shore-absorption case. The mapped-color shader/draw/GL-state-guard code
is verified unchanged. Integrated 15 FPS hand, moving-waterline, transient
terrain guard, missing-artwork, startup/sleep/push/toggle/restart tests pass.
The full openFrameworks build and hardware appearance/temperature need local
verification. Keep the current 15 FPS setting for a direct comparison.

The installer checks all three prior source hashes before writing. It makes
verified backups and updates PACKAGE-SHA256.json when present. Repeated
installation is harmless. Restore refuses to overwrite later edits and
returns to the exact preceding mapped-color renderer source, without changing operator settings.
