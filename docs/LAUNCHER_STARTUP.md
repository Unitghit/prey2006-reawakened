# Launcher startup repair

The launchers exceeded the engine's fixed 32-command startup array (35 commands for AllFixes, 36 for translated/alien text). ParseCommandLine wrote beyond that array without checking capacity, corrupting memory during startup.

Common.cpp now stores startup commands in idList<idCmdArgs>, appending a fresh entry for each command. Existing command processing and startup-variable precedence are preserved.

Release build succeeded. Muted isolated startup tests used each launcher's complete arguments with only build/profile paths substituted and mute/test commands appended (38 commands for TranslatedText, 37 for AllFixes). Both initialized the renderer/game/session and exited normally. Console captures confirm translation distances 200/0, g_nightmare 1, r_glowStrength 0.5 and s_volume_dB -60. Test script and logs are under validation/launcher-startup.

All six current launchers target validation/launcher-build, containing the rebuilt engine and matching packages with previous fixes retained.
