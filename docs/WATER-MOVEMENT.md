# Water movement, attribution and licenses

## Evan Wallace's surface-ripple algorithm — MIT

The surface-ripple update in `src/World/WaterRippleGrid.h` is adapted from
**Evan Wallace's WebGL Water**, specifically the update in `water.js`.
Copyright 2011 Evan Wallace. The upstream work is released under the
**MIT license**. The copyright and full permission/warranty notice are
retained in [`../licenses/evan-water-MIT.txt`](../licenses/evan-water-MIT.txt)
and must accompany copies or substantial portions of the adapted component.

- Upstream source: https://github.com/evanw/webgl-water
- Upstream algorithm: https://github.com/evanw/webgl-water/blob/master/water.js
- Original demo: https://madebyevan.com/webgl-water/
- The upstream source snapshot supplied with the ripple adaptation is kept at
  `docs/water-ripples-v60/upstream-water.js`, when that provenance bundle is present.

The adaptation uses a small CPU ripple grid rather than the browser demo's
full rendering pipeline. Wet boundaries, depth weighting and directional
push handling were developed for this sandbox. The original demo's images,
pool environment, ray tracing and sphere simulation were not imported.
Existing caustic-artwork credits remain in `ASSETS.md`.

## Project-specific rain, pooling and surge movement

`src/World/RainWaterGrid.h` is the project's separately written hydraulic
solver. It was not copied from Evan Wallace or from FullSWOF, LISFLOOD-FP or
Wflow. Its existing **GPL-2.0-or-later** source notice is retained. The code
uses a local-inertial approximation with conservative shared-face transport,
plus a slow thin-film law for sub-millimetre rain. Numerical references are
method citations, not claims that those projects supplied this source:

- Bates, Horritt & Fewtrell (2010), *A simple inertial formulation of the
  shallow water equations for efficient two-dimensional flood inundation
  modelling*, https://doi.org/10.1016/j.jhydrol.2010.03.027
- De Almeida, Bates, Freer & Souvignet (2012), *Improving the stability of a
  simple formulation of the shallow water equations for 2-D flood modeling*,
  https://doi.org/10.1029/2011WR011570
- Equation/CFL reference:
  https://deltares.github.io/Wflow.jl/quarto/model_docs/lateral/local-inertial.html

Development introduced stored face momentum, depth-dependent dam-break surges,
shoreline run-up/retreat and real-time momentum damping so larger waves settle
without removing their water. A later experiment multiplied hand and pressure
inputs by ten; subsequent updates revised forcing and shoreline handling. The
historical development notes describe those experiments, rather than a fixed
specification of the current solver. These are interactive teaching effects,
not an engineering flood or tsunami model. Available volume, friction, terrain
and positivity limiters determine actual run-up height.

`src/World/InteractiveWater.h` combines the ripple detail and hydraulic
heights into the top-down water appearance. Its existing GPL source notice
is retained. Fine decorative ripples affect surface appearance; actual
run-up and breached-pond transport are handled by `RainWaterGrid.h`.

The economy renderer, bounded grids, at-rest hydraulic sleep and the
eight-pass transport cap remain in use. The tested operator setting is 15 FPS;
this documentation update does not change that setting or any runtime behavior.

## Original project and other components

This fork retains the original Magic-Sand / AR Sandbox author notices and
project GPL terms described in `COPYING` and `CREDITS.md`. The MIT notice for
the adapted ripple component does not relicense the complete application.
Other framework, addon, artwork and sound licenses remain with their respective
components. In particular, this notice does not change the thunder recording's
creator permission or attribution requirements.
