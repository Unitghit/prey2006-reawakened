# Input and camera timing

The unlocked renderer previously evaluated camera interpolation using integer
milliseconds. Each 16 ms simulation interval therefore had only 1/16 blend steps.
Its frame limiter retained fractional deadlines but used integer time to wait.

Presentation now uses SDL's performance counter, anchored once to the existing
millisecond epoch. The initial phase uncertainty is less than approximately one
millisecond under normal scheduling and remains constant, rather than quantizing
every frame. Physics, command-angle encoding, save formats, and simulation tick
duration are unchanged. SDL 1 builds retain the existing millisecond fallback.

The limiter uses the same fractional clock, sleeps for whole milliseconds, and
yields for the remaining fraction. The yield loop may consume some additional CPU
near deadlines, especially at high caps; this is not a GPU presentation timestamp
or a guarantee of perfectly even displayed frames.

Unlocked single-player input is collected again after the limiter wait, before
building commands. This pass does not execute the command buffer. Async input and
event journaling retain their previous collection paths. The late mouse preview
remains disabled; the change does not offset the crosshair independently of aim.

## Validation

Development and private release builds succeeded, as did native launcher checks.
Muted, isolated portal_bloom tests used a diagnostic constant 30 degree/second
turn with late preview disabled, at 960x540 with VSync off. The middle 80% of
per-frame rendered angular speeds, measured against a performance counter:

| Cap | Before | After (final code) |
| --- | --- | --- |
| 144 FPS | 27.40 to 32.58 degrees/sec | 29.87 to 30.21 degrees/sec |
| 240 FPS | 26.15 to 33.93 degrees/sec | 29.87 to 30.21 degrees/sec |

The old interpolation fractions were all on the 1/16 grid; the new fractions are
continuous. All sampled turn frames interpolated with late preview off. These
short diagnostic traces establish a camera-generation improvement, not physical
mouse-to-photon latency. Concurrent build activity affected the final 144 FPS
frame intervals; angular speed normalized against elapsed time stayed stable.

Muted visualjolt and farportal movement tests completed with normal process exits.
Position traces confirmed travel between portal areas; sampled transition captures
showed the world and weapon without a black frame. This is limited regression
coverage, not proof of every possible portal crossing or input interaction.

Physical mouse feel still requires user testing, including slow sweeps and fast
shakes. DPI/count granularity and packed command angles still exist. Controller
response curves were not changed.

When com_fpsTrace is enabled, CAMERA_TIMING lines record presentation milliseconds,
scheduled tick milliseconds, asynchronous tick index, consumed game tick index,
and blend fraction for correlation with fps-presentation.csv. Tracing is off in
normal play. Local test artifacts live outside the source repository.
