# Linux build and dependency notes

## Tested base and current reproducibility limit

The operator's working tree uses the openFrameworks package
`of_v0.12.1_linux64_gcc6_release`. Its existing source folder can keep its old name.
The package name does not necessarily identify the currently installed compiler version.
The extensions use Kinect v2 through libfreenect2; the old upstream README's
openFrameworks 0.9.3 and Kinect v1 instructions are historical.

The runtime export now records the working compiler/library versions, four addon
Git commits, and local compatibility changes for ofxDatGui and ofxModal.
See [the dependency record](../dependencies/README.md) and its included patches.
The exact libfreenect2 archive revision remains unknown because its working tree
contains no Git metadata. A clean checkout/build has not yet been reproduced.

## Install prerequisites

1. Obtain [openFrameworks 0.12.1](https://openframeworks.cc/download/) for Linux.
   Follow the Linux dependency/install scripts shipped with that exact release.
   This repository does not bundle the full framework.
2. Build [libfreenect2](https://github.com/OpenKinect/libfreenect2) using its Linux
   instructions and udev permissions, and confirm the Kinect works in its viewer
   before attempting TopoPlay. The working application uses the OpenGL packet
   processor. USB 3 bandwidth and permissions matter.
3. Install the addons listed below into `OPENFRAMEWORKS_ROOT/addons`. Apply the captured compatibility patches to a separate clean addon checkout
   as explained in dependencies/README.md. Official ofxOpenCv and
   ofxXmlSettings come from the matching framework distribution.

| Addon | Upstream reference | Exact working revision |
| --- | --- | --- |
| ofxCv | https://github.com/kylemcdonald/ofxCv | Recorded in dependencies/README.md |
| ofxDatGui | https://github.com/thomwolf/ofxDatGui (upstream Magic-Sand reference) | `0c2230afa68fe5dea822932f6b4c178778af9440` + supplied patch |
| ofxModal | https://github.com/braitsch/ofxModal | Recorded in dependencies/README.md |
| ofxParagraph | https://github.com/braitsch/ofxParagraph | Recorded in dependencies/README.md |
| ofxOpenCv | openFrameworks 0.12.1 distribution | Recorded in dependencies/README.md |
| ofxXmlSettings | openFrameworks 0.12.1 distribution | Recorded in dependencies/README.md |

`addons.make` is preserved from the working source and has all six names.
The bundled dlib headers under `src/KinectProjector/libs` stay in the project.
Do not add the old ofxKinect addon expecting it to replace the v2 driver integration.

## Place and build TopoPlay

Source may remain in the existing openFrameworks apps/myApps folder, or use an
explicit framework path. The source folder name does not determine the executable.

```bash
bash scripts/build.sh --of-root /path/to/openFrameworks \
    --freenect2-root /path/to/libfreenect2
```

`configure.py` first uses explicit flags/environment, then saved machine-local
paths and bounded sibling detection. A system libfreenect2 pkg-config install is
also supported. For unusual layouts add `--freenect2-build /path/to/build` and
`--freenect2-lib-dir /path/to/lib`. No Downloads directory is assumed.
Machine-specific paths are recorded in config.local.make/topoplay-build.json and
excluded from public bundles. Keep source/dependency paths free of spaces and make
special characters; installed runtime paths may contain spaces.

## Runtime data and launch

Keep runtime media in bin/data. Launch with `./TopoPlay` or `bash scripts/run.sh`.
Both routes preserve GNOME-native projector mode handling on the tested laptop.
For other desktops use `./TopoPlay --no-mode-lock` and configure display mode there.

Install a separate copy with `python3 scripts/install.py`; choose its folder.
The installed copy bundles project-private shared libraries and retains system
library/graphics/USB permission prerequisites. See the top-level README for full
bundle creation, updates, settings migration and known validation limits.

## Tested runtime and cooling configuration

The operator tested on an Ubuntu Dell Inspiron 5570 with an Intel Core i7-8550U,
Kinect v2 and a 1280 × 720 projector covering 28 × 20 inches of sand. The application
frame-rate setting was 15 FPS, with CPU turbo/boost disabled to reduce heat during
extended operation. This is a tested configuration, not a minimum specification
or a sustained-frame-rate benchmark. CPU boost was configured separately at the
machine level; the installer does not alter system power settings.

Larger projected areas and larger Kinect ROIs have not yet been validated for
performance, temperature or interaction quality. Physical dimensions alone are
not a processing-cost estimate; ROI, render resolution, simulation grids and
active effects also matter. Validate the intended larger setup on its own hardware.
