# Publishing the TopoPlay source release

The public release is source plus reviewed runtime assets, not an executable or
installer bundle. Preserve COPYING, original headers, CREDITS.md, ASSETS.md,
licenses, source modification notices and the dependency/source records.

1. Apply the source-release notice/media update and verify PACKAGE-SHA256.json.
2. Commit the complete reviewed source and notices to GitHub.
3. Create a source archive from that exact commit with `git archive`; this contains
   the current generated thunder, not the historical recorded WAV.
4. Identify the download as source requiring openFrameworks 0.12.1, addons,
   libfreenect2 and matching external dependencies. See BUILD_LINUX.md.
5. Preserve the tested hardware/28 × 20 inch projection limits in the README.

Do not include executables, shared libraries, calibration, box startup profiles,
machine paths, recordings, automatic backups or build output. Installer and
packaging scripts may remain as source tools; their presence is not a claim that
an approved binary bundle is supplied. Before any binary release, identify the
actual linked/bundled dependencies, include the required notices and corresponding
source arrangements, resolve license compatibility, and test a clean system.

The repository's earlier commits may contain the prior thunder recording. This
update does not rewrite or clear that history. A source ZIP created from the new
commit excludes that recording. Separate permission confirmation or an explicitly
planned history cleanup is needed to address old downloadable copies.
