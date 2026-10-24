# Public thunder asset and historical recording

The current `bin/data/rain/thunder.wav` is original mathematical synthesis made
for the public TopoPlay source release. It imports no recordings or audio samples.
The generator is `docs/assets/synthesize-thunder.py` (GPL-2.0-or-later); the generated
WAV is distributed under the same terms to the extent copyright applies.

- SHA-256: `49e61a7a89940e395a1ffc1977bbafd171993e6401deb9c6226384aea49811b0`
- Stereo PCM, 44,100 Hz, 16 bits, 1,539,072 frames (about 34.900 seconds).
- Main onsets: 1, 9, 21 and 26 seconds, matching the existing flash schedule.
- Generation environment: NumPy 2.3.5, SciPy 1.17.0.
- The recording's duration, format and runtime path are preserved. Sound quality
  and hardware playback of this replacement still need operator testing.

## Earlier local recording

Earlier source snapshots and the operator's installed sandbox used Friendly Free
Sounds' “Thunder Sound Effect”, identified by the operator as published April 7,
2023: https://www.youtube.com/watch?v=xK_m77VZYnc . That creator is credited here
for the earlier recording, not for the current generated WAV. The operator
reported permission for media/game use with channel credit. The original wording
could not be retrieved to establish standalone source-package redistribution,
so the recording is excluded from the current source snapshot.

Earlier Git commits can still contain that recording. Replacing it in the current
source does not erase Git history or settle permission for earlier copies. Do not
attach an old source archive containing it as the new public source release.
The separately installed sandbox is not changed by this repository update.
