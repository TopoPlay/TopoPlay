# Standard Kinect driver validation — 2026-10-25

TopoPlay was rebuilt with unmodified upstream libfreenect2 on the operator's
existing Ubuntu/openFrameworks installation. The earlier custom RGB-decoding
helper was removed from the application. The local test built its own driver
and retained the previous working installation separately.

| Check | Recorded result |
| --- | --- |
| Upstream driver commit | `fd64c5d9b214df6f6a55b4419357e51083f15d93` |
| Driver build | OpenGL enabled; OpenCL, CUDA and VAAPI disabled; CMake minimum-policy override 3.5 |
| Runtime driver | Both linker inspection and running-process mappings selected the test's newly built library |
| Startup | Saved calibration loaded and RUN started |
| Fresh calibration | Accepted 120 points; recorded depth separation 124.156 mm; mean reprojection error about 1.91 pixels |
| Color processing | Application color processing switched OFF → ON → OFF across RUN/calibration/RUN while standard driver RGB decoding continued |
| Telemetry interval | About 31 minutes, 361 samples |
| CPU package temperature | Highest recorded coretemp package value 74°C; final value 71°C |
| CPU turbo | `intel_pstate/no_turbo` remained 1 throughout the sampled interval |
| Shutdown | Kinect finished after about 0.265 seconds; graphics/windows and application resources released |
| Regression suite | Eight tests passed after isolating dependency fixtures from real-machine path overrides |
| Camera-preview follow-up | Operator confirmed color defaults in Setup/calibration with projected-map display in RUN after the separate preview update |

The uploaded logs contain dropped USB/depth packets, missing interface-image
messages, one OpenCV OpenCL cache error, and an initial raised-board rejection
before successful calibration. The build also printed a clock-skew warning.
These were not repaired by this driver change. The application subsequently
calibrated, continued running, and exited through its shutdown sequence. The
record does not establish error-free operation or compare temperatures against
the old driver under identical workloads.

This validates the standard-driver transition on the recorded laptop, not a
fresh-machine installation. The build reused existing openFrameworks 0.12.1,
addons, system packages, USB permissions, and CPU settings. A complete build
using newly obtained dependencies on fresh Ubuntu remains unverified. The
camera-preview confirmation was a separate operator test; the telemetry numbers
above describe the earlier standard-driver run.

See [installation](INSTALL_LINUX.md), [build details](BUILD_LINUX.md), and
[dependency versions/patches](../dependencies/README.md).
