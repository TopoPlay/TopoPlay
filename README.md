# TopoPlay

**TopoPlay 1.0.1** is an interactive terrain playground for a Kinect v2, projector,
and real sand. Shape mountains, dig channels, build ponds and dams, make waves,
trigger storms, erupt volcanoes, and uncover fossils. This release includes operator-tested terrain and water behavior under the
TopoPlay name.

TopoPlay is an independently maintained educational fork of
[Magic-Sand](https://github.com/thomwolf/Magic-Sand), by Thomas Wolf and
Rasmus R. Paulsen, based in part on Oliver Kreylos's UC Davis/KeckCAVES
[AR Sandbox/SARndbox](https://arsandbox.ucdavis.edu/) and Gene Kogan's calibration
work. Original notices remain in the source. See [CREDITS.md](CREDITS.md),
[COPYING](COPYING), and [ASSETS.md](ASSETS.md).

## Current distribution: source and runtime assets

The public release contains source, runtime assets, notices and build scripts.
It does not supply a compiled executable, shared libraries or a prebuilt installer.
Build TopoPlay using the [Linux build notes](docs/BUILD_LINUX.md) and install its
external dependencies separately. Original notices and component licenses are
retained; see [the source-release review](docs/SOURCE-RELEASE-REVIEW.md).

## Local installation after building

After building the executable and obtaining the runtime dependencies, open the
complete local project folder and run:

```bash
python3 scripts/install.py
```

Choose the parent folder in the folder picker; the installer creates a `TopoPlay`
folder there. If no graphical picker is available, it asks in the terminal.
For an exact path, including one with spaces:

```bash
python3 scripts/install.py --destination "$HOME/Applications/TopoPlay"
```

No Downloads location is assumed. The installer creates a **TopoPlay** application
menu entry and records the chosen location for updates. It does not add anything
to startup, change CPU settings, or install system drivers. The program, assets,
source, and notices are copied together. Your source/build folder can stay elsewhere.

The executable is always `bin/TopoPlay`; start it through the root `TopoPlay`
launcher or the menu entry. The launcher resolves paths relative to itself and
preserves the tested GNOME projector/lid handling. On another desktop use
`./TopoPlay --no-mode-lock` after configuring the projector externally.

The Linux bundle still needs a compatible system-library ABI, graphics driver,
Kinect USB permissions, and USB 3 connectivity. Packaging from the working laptop
is supported; a clean-machine installation remains to be tested. An update ZIP
containing only changed source files is not a complete application bundle.

## Play and customize

- Terrain textures, adjustable elevation layers, linked layer editing, rock/grass
  mix, snow coverage/opacity, and transition softness.
- Fish, sharks, stingrays and uphill-facing pine trees; lava can burn trees.
- A drifting cloud: hold a raised hand at it to grow a thunderstorm. Rain saturates
  sand, flows downhill, pools behind dams, and drains when a channel opens.
- Hand-swept waves, shoreline run-up, gentle background ripples, and white water
  reflections over your chosen water colors.
- Height-triggered volcanoes with countdown, eruption sound, smoke, flowing lava,
  and reset after the triggering mound is lowered.
- Fossil and Dig modes with projected bone/shovel controls and GUI lockouts.
- Idle demonstrations with optional sound, adjustable gesture ceiling, inch/mm
  display, projector color correction, and saved box settings.

Water effects are interactive approximations, not quantitative hydrology tools.
Evan Wallace's MIT-licensed ripple component is credited separately in
[water documentation](docs/WATER-MOVEMENT.md); the whole application is not MIT.

## First use and settings

Keep the camera and projector rigidly mounted. For a new box, use this order:

1. Define the **manual sandbox ROI** in the camera/depth view.
2. Click **Save box settings before starting projector calibration**.
3. Run the projector/Kinect calibration for your box.
4. Click **Save box settings again**, then check the terrain against the box edges.
5. Set terrain elevations and gesture controls, and save those settings as needed.

Saving the manual ROI first matters: the fresh-install test produced a smaller,
skewed terrain footprint when that ROI had not been saved before calibration.
The tested build requests RUN automatically when saved calibration is present
and depth is ready. Public source snapshots omit the operator's private box
calibration and startup profile.

Mutable settings are in `bin/data/settings`, user terrain maps in
`bin/data/colorMaps`, and projector/performance settings alongside them under
`bin/data`. An installed folder must be writable by its user. Updates preserve
those settings and terrain maps and retain a backup of the previous installation.
The existing administrator-installed CPU helper keeps its legacy technical name
for compatibility; it is not installed or reauthorized by this user-level installer.
Frame-rate controls remain available without that optional helper.

## Projected virtual buttons

The **bone** toggles Fossil mode; the **shovel** toggles Dig mode. These are
Kinect gestures, not touch-screen buttons. Make a circle with your thumb and
finger over the icon, with its opening facing the Kinect. Hold the ring above
the configured **Landscape ceiling**, leaving the sand visible through its
center, for the **Button hold sec** interval (normally one second). Move your
hand away briefly before using it again. A flat hand covering the center is
not the intended gesture.

Use **Fossil button displayed** and **Dig button displayed** in the operator
controls to show or hide them. Hiding a button also turns off its mode. Fossil
and Dig are mutually exclusive. The icon is green while its mode is active;
a yellow outline indicates the ring gesture is being detected. See
[the full virtual-button guide](docs/VIRTUAL-BUTTONS.md) for placement, height,
hold settings and troubleshooting.

## Build from source

The source is C++ using **openFrameworks 0.12.1**, libfreenect2, OpenGL/OpenCV, and
the addons in `addons.make`. Install those build dependencies separately. See
[BUILD_LINUX.md](docs/BUILD_LINUX.md) for the recorded addon compatibility notes.

```bash
bash scripts/build.sh --of-root /path/to/openFrameworks \
    --freenect2-root /path/to/libfreenect2
./TopoPlay
```

Dependency locations are detected or explicitly recorded in machine-local
`config.local.make` and `topoplay-build.json`; they are not public defaults.
The old `../../..` openFrameworks layout remains a fallback for direct make use.
Build paths cannot contain spaces due to openFrameworks make-list limitations;
installed application paths can. No source-folder rename is required to produce
`bin/TopoPlay`.

## Updates, moving, and removing

From an extracted newer bundle run `python3 scripts/update.py`; it uses the
recorded install folder, or asks if there is more than one. Alternatively run
`install.py --destination /chosen/TopoPlay`. Close the app first. To move it,
install from the current folder to the new destination, test it, then retain or
remove the old copy yourself. To refresh a menu entry after a manual move, run
`python3 scripts/install.py --register-only` in the moved folder.

Run `python3 scripts/unregister.py` to remove that installation's menu entry and
registry record. It deliberately leaves the program and your settings intact.
Back up `bin/data` before manually removing an installation folder.

## Experimental binary packaging for local testing

The current public release is source-only. Binary redistribution needs a separate
license/dependency review and a clean-system test. After a successful local build:

```bash
python3 scripts/package-release.py --output /chosen/output/TopoPlay-1.0.1-linux.tar.gz
```

This collects your actual executable, all runtime media, private shared libraries,
source and notices into an installable bundle. System libraries/graphics drivers
remain external. Calibration, box startup settings and machine-specific color/CPU
preferences are omitted by default; use `--include-box-settings` only for a private
same-box copy. Do not publish a private box bundle by mistake.

## Tested setup and performance limits

| Item | Tested configuration |
| --- | --- |
| Computer | Dell Inspiron 5570 running Ubuntu, Intel Core i7-8550U |
| Depth camera | Kinect v2 |
| Projector resolution | 1280 × 720 pixels |
| Projected sand area | 28 × 20 inches (approximately 71 × 51 cm) |
| Application frame-rate setting | 15 FPS |
| CPU turbo/boost | Disabled to reduce heat during extended operation |

These are the operator's test settings, not minimum hardware requirements or a
performance guarantee. The 15 FPS value is the configured target, not a published
benchmark of sustained frame rate. CPU boost is a machine-level cooling choice;
the TopoPlay installer does not disable boost or change system power settings.

Performance, temperatures, tracking and interaction quality with larger projected
areas or larger Kinect regions of interest (ROIs) have not yet been validated.
Physical area alone does not establish processing cost: camera ROI, rendering
resolution, simulation grid sizes and enabled effects also matter. Test a larger
setup with its intended calibration and effects before relying on it for an event
or classroom session.

The slight terrain refresh jitter improved with the material-height cache and
steadier tree placement. The TopoPlay name migration preserves that reviewed
rendering and water simulation. Windows/macOS and clean-machine distribution
have not been validated. Historical development notes retain their filenames
so source references remain valid.

Thunder credit: **Friendly Free Sounds**, “Thunder Sound Effect”, identified by
its supplied source as published April 7, 2023:
https://www.youtube.com/watch?v=xK_m77VZYnc . The original WAV is retained with
channel credit based on the operator's supplied permission and confirmation of
this project's use. See [the sound record](docs/assets/THUNDER-PERMISSION.md).

## Closing TopoPlay

Close the operator GUI with its X button to quit the whole application, including
the projector window and Kinect acquisition. Minimizing the GUI or closing the
laptop lid does not request an application exit; the existing GNOME projector
handling continues. Check with `pgrep -a -x TopoPlay` after closing the GUI.

## Optional desktop shortcut

The installer asks whether to create a desktop shortcut. Declining keeps the
application-menu entry. Use `--desktop-shortcut` or `--no-desktop-shortcut` for an
explicit choice; noninteractive runs skip creating it unless requested. Existing
installer-managed shortcuts are refreshed on updates. No startup entry is added.

To add the shortcut later, from the installed folder run:

```bash
python3 scripts/install.py --register-only --desktop-shortcut
```

The installer uses your configured desktop folder and marks the launcher executable.
On Ubuntu you may need to right-click it and choose **Allow Launching**. Desktop
icons must be enabled by the desktop environment; the installer does not change
extensions or system preferences. The shortcut uses the system education icon.
Removing registration also removes this installation's managed desktop shortcut.
