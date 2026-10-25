
## Interactive water ripples

The surface ripple implementation in `src/World/WaterRippleGrid.h` is adapted from
Evan Wallace's WebGL Water (`water.js`), Copyright 2011 Evan Wallace,
released under the MIT license. Source: https://github.com/evanw/webgl-water
and https://madebyevan.com/webgl-water/ . See [the MIT notice](../../licenses/evan-water-MIT.txt) and the
[upstream source copy](upstream-water.js).

This adaptation uses a bounded CPU grid and a lightweight top-down shader.
It does not include the demo's pool tiles, environment images, ray tracing,
reflection/refraction scene, sphere simulation or external JavaScript libraries.
The pool-caustic artwork is credited in [Assets](../../ASSETS.md).
Original Magic-Sand / UC Davis attribution and GPL notices remain unchanged.
