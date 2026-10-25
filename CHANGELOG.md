# TopoPlay change log

## Standard Kinect driver and camera preview defaults — 2026-10-25

- Remove the custom libfreenect2 RGB-decoding helper dependency.
- Keep standard driver decoding active; skip unneeded TopoPlay color work and release unused RGB frames in RUN.
- Preserve attached listeners, streaming lifecycle, and calibration color registration.
- Default Setup/calibration to the color camera on mode entry; retain manual depth/color selection within each mode and projected-map display in RUN.
- Record the standard-driver hardware test and pin its upstream revision in the completed Ubuntu installation guide.
- Include the CMake policy compatibility option and isolate dependency test fixtures from real-machine environment variables.

## Ubuntu installation guide — 2026-10-25

- Document source downloads, system libraries, openFrameworks, recorded addons/patches, Kinect permissions/testing, building, calibration, installation, updates, and troubleshooting.
- Add a prerequisite table and installation-guide links to the main README and dependency/build documents.
- Identify the unpublished custom libfreenect2 symbol as a fresh-install blocker; do not claim a stock driver or the current preflight verifies this requirement.

## Build dependency guidance — 2026-10-25

- Check common build tools, framework/Kinect paths, addon sources, and development libraries before configuring or compiling.
- Name missing dependencies and suggest Ubuntu/Debian package commands and framework/driver setup instructions without installing anything automatically.
- Document the standalone read-only check and its limits; a fresh Ubuntu build remains unverified.

## User-selected installation and shortcut — 2026-10-24

- Explain the installation folder selection in a dialog before opening the picker; Cancel stops the installation.
- Open the normal installation folder picker at Home and wait for the user's parent-folder choice, independently of old registered installations.
- Pass Home explicitly to the Zenity fallback so a remembered removed folder is not used as the initial location.
- Ask whether to create or update the desktop shortcut even when a managed shortcut already exists.
- Retain explicit command-line flags, the logo icon, saved-setting preservation on updates, and unrelated application behavior.

## TopoPlay launcher icon — 2026-10-24

- Bundle the original TopoPlay logo as the desktop/application-menu icon.
- Install a stable per-user icon when creating or refreshing launchers; use it for both menu and optional desktop shortcuts.
- Preserve existing launcher commands, calibration, rendering and fossil behavior.

## Fossil variety and buried logo — 2026-10-24

- Add generated Triceratops, Pteranodon and three-skeleton-group sprites, plus a transparent version of the TopoPlay logo, alongside the existing T. rex.
- Choose one image, a 0–360 degree angle, and 65–100% of its safely fitted size each Fossil-mode round; avoid immediate image repeats when another readable image exists.
- Share the selected image and scale between buried-reference sampling and drawing; keep the selection fixed during digging.
- Discover additional PNGs in `bin/data/dig/fossils` when a new round starts; preserve the existing T. rex fallback.
- Include generated-art provenance and user instructions. Groups reveal through the existing mask and share one buried reference plane.
- Operator tested the new fossil variety and buried logo in a separate installation. Calibration, synchronization, Dig mode and demo effects remain unchanged.

## Projector synchronization — 2026-10-24

- Synchronize the finished RUN image between the operator and projector graphics contexts, preventing a new frame from overwriting a texture still being read by the projector.
- Drain the previous RUN handoff once when returning to Setup/calibration; leave calibration rendering outside the synchronization path.
- Release frame synchronization resources before graphics-context teardown.
- Keep the rebuilt calibration implementation and current scene layer order unchanged.
- Operator testing: no sprite flicker observed in the separate synchronization test; observation duration was not recorded. Results apply to the tested setup.
- Known issue under review: ocean ripples appear inactive during idle demos. This update does not change demo effects.

## Fresh calibration rebuild — 2026-10-24

- Start every calibration attempt with 120 fresh point pairs; do not accumulate failed attempts.
- Wait for distinct color frames and stable refreshed depth at each pattern and height change.
- Resolve checkerboard row/column ordering and use a normalized projective fit.
- Require two separated measurement heights and reject excessive mean/worst projection error.
- Save Kinect and projector diagnostic point lists to separate files.
- Restore the rendering composition used during the successful hardware tests; retain responsive shutdown. This supersedes the earlier shared-context/layer experiment below. Intermittent sprite flicker remains under investigation.
- Operator validation: two fresh calibrations succeeded on the first attempt; the first logged run averaged 1.56 pixels error, worst 5.16 pixels, and exited normally. Saved calibration also reopened aligned. This is validation on the tested setup, not a fresh-OS dependency test.

## Tested projector and shutdown fixes — 2026-10-23

- Separate animals, ripples, lava, trees and overhead clouds into explicit scene layers.
- Synchronize the projector texture between the operator and projector OpenGL contexts.
- Keep the GUI responsive while Kinect acquisition stops, then release shared graphics work before closing.
- Operator test: no flicker observed with ripples enabled; closing took about three seconds. Results apply to the tested setup.

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
