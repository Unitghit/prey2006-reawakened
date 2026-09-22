# Force Cherokee on save load

Prey Settings > Controls & gameplay > Force Cherokee on save load controls
archived boolean g_forceCherokee. The engine default is off to respect existing
save difficulty. Enabled in this user's settings and custom launcher by request.

After reading the saved g_wicked value, single-player save restoration overrides
it with 1 when enabled. Disabled leaves the saved value intact. This does not
rewrite existing saves; subsequent saves naturally record the active difficulty.
It does not force new games or multiplayer difficulty.

Release build and configurator publish/layout/serialization verification passed.
Muted isolated test made a Normal fixture, loaded it with the option enabled
(g_wicked 1), then disabled the option and reloaded the same fixture (g_wicked 0).
See validation/cherokee/fixed/base/result.log. User save files were untouched.
Settings launcher now targets validation/cherokee-build.
