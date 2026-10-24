# Mapped-color ripple rendering and hand reference — V71

The uploaded terrain1.xml/profile remains unchanged. Its edited regular
waterline is 31.154657 mm. Changing terrain levels is supported; V71 does not
restore defaults, change layer spacing, or replace box settings/calibration.

## Water appearance
The uploaded renderer painted fixed blue-green water over the terrain and
sampled a caustic image. Disabling ripples removed that rendering path, which
explains the different base color. V71 instead adds neutral transparent light
and shade over the existing mapped sea color. It uses a small normal map only;
no caustic image, tiled artwork, fixed sea-blue fill or image load is required.
Rain pools retain their existing blue base mesh on both sides of the ripple
toggle, with ripple lighting added when enabled. Existing artwork files and
credits are left intact, although this water path no longer samples the image.
Ripples remain masked to water; this does not put water effects over all land.

## Flicker investigation and drawing isolation
The user's cloud/tree/demo-hand flicker stops with ripples disabled. This
isolates the implicated rendering path but does not, by itself, identify one
exact leaked state on the laptop. V71 scopes the water pass's graphics state:
view/style, texture bindings, active unit, shader program, blend factors and
equations, color/depth write masks, depth/cull/stencil/scissor enables. The GL2
compatibility branch uses attribute stacks; the GL3 branch restores explicit
state. Incoming scissor and write masks cannot clip the water buffer's clear.
Both direct and quarter-resolution paths use the guard. Only one texture unit
is needed. Demo engines, cloud geometry, tree artwork and display mode logic
are unchanged. Hardware testing is needed to confirm that the flicker is gone.

## Hand-push consistency
The uploaded log contains many accepted WAVE PUSH entries (hydraulic=1), so
not every weak-looking wave was a detection failure. There is also a concrete
height-reference interaction: the old code measures hand clearance above the
instantaneous crest. V71 uses the edited regular sea waterline instead; perched
rain ponds follow a slowly filtered local surface. Water eligibility uses
physical storage rather than the rendered color/opacity threshold.

The newest existing CPU depth snapshot is checked each app update rather
than waiting for cloud/terrain scans. This is a CPU snapshot copy, not another
Kinect acquisition or GPU readback. A moving, recognized hand protects its
occupied terrain cells for a short 0.6-second grace, so it cannot immediately
become a temporary mound. A stationary changed surface can then be sampled
again; permanently held objects are not hidden forever. Existing wave gesture
height/speed limits, high-ceiling ring/rain controls, 10x forcing, V70 shore
absorption, physical rain/pool storage and the eight-pass cap are unchanged.

## Validation and limits
Actual header tests reproduce old rejection for a hand 60 mm above the uploaded
edited waterline during an 80 mm crest; V71 accepts the moving hand. Short
terrain guard expiry and missing-artwork setup also pass. Integrated RainRunoff
rest/push/restart/toggle tests and 15 FPS hand guards pass. All eight actual
GLSL/backend/sampler paths compile/link and animate. The neutral overlay has
no blue tint; flat water preserves several mapped base colors within normal-map
quantization/breeze tolerance. Actual C++ DrawState code was tested against
Mesa for repeated GL2/GL3 state restoration (OF style-pop behavior emulated).
Offscreen premultiplied alpha/compositing and quarter-resolution checks pass.

These checks are not a full openFrameworks/Kinect/projector hardware run.
Check normal and demo flicker, push consistency and temperature locally at
15 FPS. The existing <=640x360 lighting buffer and bounded grids are retained.
The lighter shader removes caustic image fetches; there are extra state guards
and more frequent CPU snapshot checks. No hardware speedup is promised.

V69 Evan Wallace/MIT credits and existing project licenses remain intact.
Only InteractiveWater.h and RainRunoff.cpp executable source are replaced;
this note is added. No ofApp/GUI, saved map, calibration, source solver or
asset files are overwritten. Source hashes are tied to the uploaded archive.
