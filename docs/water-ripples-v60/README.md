
## Interactive water ripples

The surface ripple update in `src/World/WaterRippleGrid.h` is adapted from
Evan Wallace's WebGL Water (`water.js`), Copyright 2011 Evan Wallace,
released under the MIT license. Source: https://github.com/evanw/webgl-water
and https://madebyevan.com/webgl-water/ . The retained MIT notice is in
`licenses/evan-water-MIT.txt`; the original downloaded source is retained in
`docs/water-ripples-v60/upstream-water.js` for provenance.

This adaptation uses a bounded CPU grid and a lightweight top-down shader.
It does not include the demo's pool tiles, environment images, ray tracing,
reflection/refraction scene, sphere simulation or external JavaScript libraries.
Water artwork remains the pool-caustic image credited in ASSETS.md.
Original Magic-Sand / UC Davis attribution and GPL notices remain unchanged.
