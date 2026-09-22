# Scoped aiming sensitivity

The weapon supplies scope FOV / g_fov through SetViewAnglesSensitivity.
Pass this factor directly to the input system, matching the existing weapon
implementation and openPREY source. The former custom 0.5 multiplier and
tangent curve are removed. At normal FOV 90, scope FOVs 20, 15, 10 and 5
produce factors 0.222222, 0.166667, 0.111111 and 0.055556.

The common-to-usercmd handoff remains active for mouse, controller look and
late-mouse preview. Per-frame refresh preserves save-load handling. GUI cursor
movement is unaffected. This restores the source behavior; it is not a claim
of a new disassembly verification of the retail executable.
