# Publishing TopoPlay

The permanent product/executable name is TopoPlay (initial public version 1.0.0).
Retain original project and third-party notices in CREDITS.md, COPYING, ASSETS.md,
licenses and source headers. Historical docs and technical namespaces may still
refer to Magic-Sand/AR Sandbox; these are attribution and compatibility identifiers.

First build/test the renamed app on the operator's hardware. Create the full Linux
bundle with scripts/package-release.py, without --include-box-settings for public
use. Test installing it in a folder outside Downloads (also one containing spaces)
on a clean compatible Linux system. Verify Kinect permissions and dependencies.
The small TopoPlay rename patch itself contains neither all media nor an executable;
it is a migration tool, not the complete release artifact.

Use the complete current source, addons.make, portable config.make, scripts, docs,
licenses and assets for GitHub. Exclude config.local.make, topoplay-build.json,
calibration.xml, kinectProjectorSettings.xml, box-startup.json, recordings, backups
and build objects. Do not publish the private final backup. The package helper's
public bundle omits box-specific startup/calibration/color/CPU files by default.

Dependency/addon revisions and compatibility patches are documented in the existing
dependencies record, when present in the working tree. Resolve that record's
remaining clean-build/media-provenance limitations before a public release claim.
