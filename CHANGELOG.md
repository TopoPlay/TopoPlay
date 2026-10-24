# TopoPlay change log

## TopoPlay 1.0.1 — shutdown fix

Closing the operator GUI now requests exit of the complete two-window application.
Kinect acquisition receives a stop request before window teardown. Lid and minimize
behavior, rendering, physics, calibration and performance settings are retained.
The installer/package scripts carry the rebuilt executable into installed copies.

## TopoPlay 1.0.0 — packaging and name migration

- Permanent TopoPlay executable/window/menu branding based on operator-tested terrain and water behavior.
- User-selected install folder, self-relative runtime paths, recorded build
  dependencies, bundled private runtime libraries, update/move support.
- Preserve calibration, terrain maps, saved settings and original attribution.
- No terrain, animal, wave, gesture, CPU-policy or projector-display logic changes.
- The optional projector handoff experiment is not added or removed by this migration.

## Historical development record

### GitHub preparation — October 21, 2026

- Collects the working educational-fork source and reviewed core runtime media.
- Includes source/build/dependency notes, attribution and asset records.
- Excludes machine calibration, saved settings, backups, executables and logs.
- Adds package verification and optional comparison with the working installation.
- No new simulation, calibration or performance behavior changes.

### Educational fork development

- Storm clouds, rain saturation, persistent pools and dam drainage; animated water.
- Height-triggered volcanoes, lava distortion, smoke, audio and burning trees.
- Wildlife refinements, stingrays, fish size limits and terrain-aware pine trees.
- Fossil/Dig controls, projected bone/shovel icons, ring gestures and mode lockouts.
- Idle demos, demo sound control, saved box settings, units and operator layout.
- Grass/rock foothill mixing and adjustable snow coverage/opacity, including a full cap.
- Restored Friendly Free Sounds thunder with attribution; generated replacement media
  and licensed DejaVu font aliases; retained upstream code/dependency notices.

The development changes were tested iteratively on the operator's box;
this is not a claim of identical results on untested hardware or a fresh OS install.
