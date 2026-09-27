# Vehicle presentation regression

Vehicle cameras previously opted out of the unlocked renderer's presentation
interpolation, also disabling world and cockpit smoothing during flight. Allow
same-vehicle first-person poses to interpolate using the existing view/world
settings. Track the actual vehicle through an entity pointer; entry, exit,
replacement and history resets cannot blend across different vehicles. A vehicle
teleport invalidates presentation history. Physics, thrust, weapon timing and
input remain fixed-tick. On-foot late-mouse prediction is not applied to vehicle
steering. The new history is transient and does not change save formats.

Use `vehicle_presentation.cfg` with the private vehicleweird save in a muted,
isolated hidden-desktop profile. Set com_unlockedFPS 1, com_fixedTic 0,
com_maxFPS 144, and s_volume_dB -60. The test exercises docked exit, on-foot
rendering, reload into the shuttle, translation/turning, save/reload and firing.
Run a separate legacy session with com_unlockedFPS 0 to check fixed-tick output.

fps-presentation.csv now includes vehicle entity number (-1 on foot) and the
vehicle_transition flag. Transition frames must not interpolate. Same-tick
render frames during turning must have distinct render_yaw values despite a
constant sim_yaw. Existing physics assertions now also verify vehicle origin,
axis and velocity remain unchanged after rendering.

Observed in the hidden 144 FPS movement test: baseline 0/579 interpolated
frames; updated 462/464, with two warm-up frames. Exit and save/reload tests
passed; a separate legacy test retained 0/133 interpolated frames. No vehicle
physics assertion fired. No retail data or personal saves are included.
