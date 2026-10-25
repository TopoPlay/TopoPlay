# Build dependencies

TopoPlay uses openFrameworks 0.12.1, standard libfreenect2, and six openFrameworks
addons. Start with the [Ubuntu installation guide](../docs/INSTALL_LINUX.md)
for download links and commands in installation order.

## Framework and Kinect driver

| Component | Version or revision |
| --- | --- |
| openFrameworks | 0.12.1 Linux 64-bit release package |
| libfreenect2 | `fd64c5d9b214df6f6a55b4419357e51083f15d93` |

Build the driver with OpenGL enabled. The installation guide includes the CMake
policy compatibility option needed by newer CMake versions. No TopoPlay-specific
Kinect driver patch is required.

## Addons

Install addons in your openFrameworks `addons` folder. Use the revisions below
and apply the two supplied compatibility patches.

| Addon | Revision | Additional step |
| --- | --- | --- |
| ofxCv | `209a3986a8f8e1928c7a11f6e1c5d8ea5406e639` | None |
| ofxDatGui | `0c2230afa68fe5dea822932f6b4c178778af9440` | Apply `patches/ofxDatGui.patch` |
| ofxModal | `71bfaa283407bbe58b921fd581d8665f21cf5eb8` | Apply `patches/ofxModal.patch` |
| ofxParagraph | `41dbfd2e07e00e358629d470dd1b6f986dee2b70` | None |
| ofxOpenCv | Included with openFrameworks 0.12.1 | Use the framework's copy |
| ofxXmlSettings | Included with openFrameworks 0.12.1 | Use the framework's copy |

Download and checkout commands are in the installation guide. `addons.make`
selects these six addons for the build.

## Apply the compatibility patches

Run each check before applying its patch. Replace `/path/to/TopoPlay` and
`/path/to/openFrameworks` with your source/dependency locations.

```bash
cd /path/to/openFrameworks/addons/ofxDatGui
git apply --check /path/to/TopoPlay/dependencies/patches/ofxDatGui.patch
git apply /path/to/TopoPlay/dependencies/patches/ofxDatGui.patch
```

```bash
cd /path/to/openFrameworks/addons/ofxModal
git apply --check /path/to/TopoPlay/dependencies/patches/ofxModal.patch
git apply /path/to/TopoPlay/dependencies/patches/ofxModal.patch
```

If a check fails, stop: the addon may be at another revision or the patch may
already be applied. Do not force a patch or reapply it to a working dependency.

The ofxDatGui patch addresses standard-library calls and theme initialization.
The ofxModal patch handles window callbacks, listener cleanup, and animation
timing. Included diagnostic logging is retained.

## Reference files and licenses

`tested-addon-sources/` contains reference copies of addon source and notices.
These are not complete addon distributions: external libraries and some data
are omitted. The build uses the addons installed under openFrameworks.
The reference copies include dated TopoPlay notices; applying a compatibility
patch reproduces its functional changes, not those later notice annotations.

The openFrameworks and addon license texts are in [licenses](../licenses/).
Keep the original notices with separately downloaded dependencies.

## Validation

The recorded environment includes Ubuntu 26.04.1, GCC 15.2.0, GNU Make 4.4.1,
OpenCV 4.10.0, libusb 1.0.29, and libturbojpeg 2.1.5. These describe the tested
system; they are not all minimum version requirements.

See [the validation record](../docs/STANDARD-DRIVER-VALIDATION.md) for the
standard-driver hardware test. A complete fresh-system build remains unverified.
