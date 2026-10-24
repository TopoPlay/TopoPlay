# TopoPlay release checklist

1. Keep the operator's V78 final backup intact.
2. Build bin/TopoPlay with scripts/build.sh; test calibration, water, modes, saved
   settings, demo, CPU behavior and projector/lid handling on the original box.
3. Create a public bundle with scripts/package-release.py --output /chosen/file.tar.gz.
4. Extract and run scripts/install.py; select the intended install folder.
5. Test the complete bundle on a clean compatible Linux system, including a path
   containing spaces. Check dependencies, Kinect USB permissions, menu launch,
   settings persistence, update preservation and optional mode-lock behavior.
6. Include source, original notices, GPL text, component licenses, asset credits
   and the recorded dependency/addon patches. Follow docs/PUBLISHING.md.

A successful same-machine copy is not a cross-distribution validation. The build
machine's Linux/libc/architecture are recorded in RELEASE.json. The separate V77
projector handoff experiment remains a known independent change/test.
