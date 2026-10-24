# Source release review

Prepared 2026-10-23 from the supplied 494-file TopoPlay source snapshot. This review
covers the current source/assets archive; no executable or linked libraries were
supplied for binary review. It is an evidence record, not legal certification.

## Addressed in this update

- Preserve upstream GPL-2.0-or-later grants and original file-level authorship.
- Add dated modification/source snapshot notices; do not invent earlier dates.
- Include exact openFrameworks 0.12.1 license text from
  https://github.com/openframeworks/openFrameworks/blob/0.12.1/LICENSE.md .
- Preserve dlib Boost, Evan Wallace MIT, addon MIT and DejaVu notices.
- GUI artwork: all five PNGs match ofxDatGui upstream commit
  `21dadfe612c16bf364d9bcdfe5a6055ab4eb2891` byte for byte; retain MIT notice.
- Thunder: retain the original recording with Friendly Free Sounds channel credit
  and the operator-supplied permission/project-use confirmation. The intermediate
  generated replacement is reverted; its recipe remains an optional alternative.
- Generated-image exports match the 12 recorded provenance hashes. Pine and pool
  artwork retain their separate reported generation history; original prompts for
  those two are not included. All seven font binaries identify as DejaVu.
- Remove warnings about old island/splash images absent from this source snapshot.
- Record all current media hashes and regenerate the package checksum list.
- Clarify source-only publication and unvalidated larger-ROI performance.

## Scope and remaining limits

The original thunder recording is retained by operator direction and confirmation
of its permitted project use. Its exact hash, supplied creator permission and
channel credit are preserved in the sound record. No history cleanup is requested.
This review relies on the supplied permission record; it is not an independent
legal determination of its scope.

No change to installed laptop software, calibration, CPU settings or private data
is made by applying this source-repository update. The exact original recorded audio is restored. No fresh openFrameworks/Kinect build was performed in
this review environment; code changes are comments only.

Source remains GPL-2.0-or-later where originally granted. Component licenses are
not replaced. Before distributing a linked executable, confirm actual dependency
licenses and the applicable combined-work GPL version; do not assume that the
GPL v2 text alone resolves Apache-2.0 dependency compatibility. Preserve required
third-party notices/source and validate the binary on a compatible clean system.
