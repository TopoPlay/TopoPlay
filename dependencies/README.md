# Recorded working dependencies

The operator's export verified openFrameworks **0.12.1** in `ofConstants.h`.
It reported Ubuntu **26.04.1 LTS**, GCC **15.2.0**, GNU Make **4.4.1**, OpenCV
**4.10.0**, libusb **1.0.29**, and libturbojpeg **2.1.5**. These are observed
versions, not minimum supported versions or a clean-build guarantee.
libfreenect2's supplied CMakeLists.txt declares **0.2.0**; that tree has no Git
metadata, so its exact archive revision/local changes are not established.

| Addon | Reported commit | Working modifications |
| --- | --- | --- |
| ofxCv | `209a3986a8f8e1928c7a11f6e1c5d8ea5406e639` | Tracked tree reported clean |
| ofxDatGui | `0c2230afa68fe5dea822932f6b4c178778af9440` | Five modified files; patch included |
| ofxModal | `71bfaa283407bbe58b921fd581d8665f21cf5eb8` | Two modified files; patch included |
| ofxParagraph | `41dbfd2e07e00e358629d470dd1b6f986dee2b70` | Tracked tree reported clean |
| ofxOpenCv | Matching openFrameworks addon | No independent Git metadata; source captured |
| ofxXmlSettings | Matching openFrameworks addon | No independent Git metadata; source captured |

The capture intentionally omitted remote URLs, which can contain credentials.
Repository links in the build guide come from upstream attribution; availability
of the reported commits at those specific remotes has not been independently
verified. The included `tested-addon-sources/` copies record the actual source
files and notices received, including working modifications. They are reference
snapshots, not duplicate build inputs: `addons.make` still uses OF_ROOT/addons.
They do not include each addon's full external libraries/data directories.

## Applying the local compatibility fixes

On a separate dependency checkout at the recorded base commit, check and apply:

```bash
# Set this to this project's dependencies/patches directory.
patch_dir=/absolute/path/to/Magic-Sand/dependencies/patches
cd /path/to/openFrameworks/addons/ofxDatGui
git apply --check "$patch_dir/ofxDatGui.patch"
git apply "$patch_dir/ofxDatGui.patch"

cd /path/to/openFrameworks/addons/ofxModal
git apply --check "$patch_dir/ofxModal.patch"
git apply "$patch_dir/ofxModal.patch"
```

Do not apply these to the already working laptop addons: they already contain
the changes. Check failures mean the checkout differs or the patch is already
applied; do not force it. Included snapshots can be used for comparison.

ofxDatGui changes qualify standard-library calls and avoid constructing a shared
theme before it is available. ofxModal binds callbacks to the window that opened
the modal, removes those listeners on closing, and ensures a nonzero animation
duration. The patch also preserves the operator's existing diagnostic logging.
These are the exact captured changes, not newly refactored dependency code.

## Source-release notice update

Original addon MIT notices are preserved. The five captured ofxDatGui files and
two captured ofxModal files now also carry dated, comment-only TopoPlay snapshot
notices. These notice comments were added after the captured compatibility patches;
applying the original patches reproduces their functional changes, not these later
annotation bytes. openFrameworks 0.12.1's full license text is included in
`licenses/openFrameworks-0.12.1-LICENSE.md`; addon MIT notices are also centralized
under `licenses/`. Dependencies downloaded separately retain their own licenses.
