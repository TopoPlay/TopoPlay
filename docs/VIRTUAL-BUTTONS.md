# Projected virtual buttons

## What the icons do

| Projected icon | Action |
| --- | --- |
| Bone | Toggle Fossil mode on or off |
| Shovel | Toggle Dig mode on or off |

These are the two enabled projected mode-button types in the current code.
There is no separate active projected Rain or Volcano mode button. Rain/cloud
and mound-triggered volcano interactions use their own gesture/terrain controls.

In the operator controls, enable **Fossil button displayed** and/or **Dig button
displayed**. The defaults show the Fossil control and hide the Dig control.
Hiding a control also stops its active mode. Save box settings to retain the
mode-control selections. Placement is chosen automatically using the calibrated
camera ROI and projector footprint. Keep the camera/projector mounts fixed.

## Activate a button

1. Make an O-shaped ring with your thumb and finger above the projected icon.
   Orient the opening toward the Kinect so it can see the sand through the hole.
2. Raise the ring above the configured **Landscape ceiling**. This is measured
   above the calibrated water/base level, not from an arbitrary mound of sand.
   The normal setting is around 280 mm (11 inches); check your current slider.
3. Keep the open center aligned over the icon. Hold steadily for **Button hold
   sec**, normally one second. A flat palm over the center is rejected.
4. Move the ring away briefly to release the control before another activation.
   The release requires ten missed detections, so allow roughly a second at the
   tested 15 FPS setting rather than rapidly tapping or waving.

The ring does not have to touch the sand. The detector looks for raised material
around a center that remains below the gesture ceiling; it does not recognize a
mouse click or projected-picture touch. It tolerates a partial finger circle,
but needs coverage around all four quadrants and a visible open center.

The artwork becomes green when its mode is active. A yellow outline indicates
a detected ring shape, not necessarily that the full hold interval has elapsed.

## Modes and controls

Fossil and Dig cannot run together: activating one stops the other. Starting
these modes clears the requested fish/rabbit/shark populations and disables rain
and volcano effects. This is expected mode behavior, not a rendering failure.
The projected control areas are protected from excavation placement.

Press **H** to show/hide Trigger Settings if needed. Adjust **Landscape ceiling**,
**Button ring size**, and **Button hold sec** there. Use **Apply and save triggers**
for the hold/ring settings; changes to the ceiling are also applied/saved by its
control. The ring-size setting adjusts the sensing area rather than enlarging
the projected bone or shovel artwork. Inches/mm display can be changed in the
same panel. These settings should be chosen for the children's reach and the
actual mounted camera geometry.

## If a control is absent or does not respond

- Confirm the mode button's display toggle is enabled and the application is RUNning.
- Check the saved manual ROI, then calibrate and save again. A control is not drawn
  until the code finds a valid visible placement inside the projector footprint.
- Leave the icon and the ring's center unobstructed; keep the fingers above the
  current ceiling rather than placing a flat hand on the sand.
- Hold still long enough, then release fully. Reduce the hold interval or sensing
  size through Trigger Settings if the existing values do not suit the users.
- Source references: `src/FlexibleRing.h`, `src/TriggerControls.h`,
  `src/ModeButtonSettings.h` and `updateRingButtonTest` in `src/ofApp.cpp`.
