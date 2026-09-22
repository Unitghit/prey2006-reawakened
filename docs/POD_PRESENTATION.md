# Deforming puzzle bomb presentation

The ball_bomb save's nearest bomb is hhPod_object_pod_3050. It uses idEntity::SixtyHertzCallback to refresh its surface deformation. CanPresentEntity previously excluded that callback, so the renderer displayed only fixed-tick physics poses even with world interpolation enabled.

The eligibility check now permits hhPod with that specific timer callback. The callback remains installed; transform history, quaternion interpolation, teleport/discontinuity guards, save/load reset and renderer-state restoration use the existing world presentation path. No velocity, mass, collision, explosion, trigger or simulation timing changes. Existing World smoothing in Prey Settings controls the fix.

The diagnostic fpsTestView podprobe (requires com_fpsTrace, single-player) selects the nearest visible pod for the existing presentation trace. The trace now includes XY coordinates and checks orientation as well as position for physics preservation.

During reload testing, both baseline and fixed builds exposed an unrelated access violation at RB_CreateSingleDrawInteractions when reading vLight->lightDef->parms.noSpecular. The source light can be freed before queued rendering completes. viewLight now owns a copy of noSpecular, captured in the front end, and the backend uses that copy.

Validation: muted 144 FPS runs loaded ball_bomb and pushed the bomb using the existing test movement control. Baseline: 139 traced frames, zero interpolated positions. Fixed: 139 frames, 110 interpolated positions; XYZ matches the expected interpolation to within 0.001 units. World smoothing disabled: 140 frames, zero interpolated positions. All traced frames preserved physics position and orientation. Both fixed runs saved/reloaded and reached BALL_TEST_PASS after the light-lifetime repair. Verification script: validation/ball-bomb/verify.py. Configurator layout checks also passed; saved user options and legacy launchers were preserved.
