# Moving portal destination synchronization

Reproduction: load doordesynced, move thinThingRelay back, start its return
movement, and cross thin_thing_office_portal while thin_thing is still moving.
Reloading the save alone rebuilds the views and conceals the failure.

The movement script refreshes both camera targets, but a portal outside the
player's PVS can stop presenting. hhGameLocal::Draw previously initialized
remoteRenderView only when null. Once allocated, the cached destination stayed
at its last presented position even though its bound portal continued moving.
Baseline diagnostics recorded final physical destination (420,-2368,394) with
cached view (513.71,-2368,394), persisting after the mover stopped.

Draw now refreshes each eligible portal's destination renderView every frame.
The renderer shares that persistent view object, so updating its contents is
sufficient; UpdateEntityDef is needed only when the pointer changes. This does
not move physics, change teleport transformations, or alter save formats.

The identical instrumented reproduction ended with both cached and physical
positions (420,-2368,394). Temporary entity-specific logging was removed before
packaging. Evidence is in validation/door-desync/reverse-before.log and
reverse-after.log. Tests are isolated and muted with s_volume_dB -60.

Release build and configurator verification passed. Deployment uses
validation/portal-sync-build through Prey Settings and the custom launcher.
Existing user choices and legacy launchers are preserved.

Final packaged-build validation: crossing during movement and returning through
the stopped door both reported render_bias 0 and landing_error 0. The return
fixture uses setviewpos eye height 388.25 (not foot height). threeportals and
boxrender loaded and rendered successfully afterward; process exited normally.
