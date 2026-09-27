# Live camera particle clock

The `videocall` save in Jen exposed frozen explosion particles behind the
priestess. R_RemoteRender copied the camera's cached renderView time, while its
inherited floatTime and the animated actors continued advancing. The camera
timestamp could remain unchanged for over 800 ms, then jump when republished.

Remote render-to-texture views now inherit the current parent view's time.
Camera position, orientation, FOV and shader parameters are preserved. This
keeps particle age consistent with material time without requiring game-side
entity updates every rendered frame. Portal and mirror paths are unchanged.

Hidden muted playback verified the explosion-to-smoke progression, with regular
and 144 FPS capped rendering. `r_portalTrace 1` logs REMOTE_CLOCK with cached
camera time and the time actually used; used must match parent on every frame.
The local reproduction save and retail assets are not distributed.
