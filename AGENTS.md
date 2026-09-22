# Reawakened development

Keep retail archives, activation keys, personal saves, captures, and compiled outputs out of Git. Run tools/audit-source.py before committing release work. Do not import the sibling development workspace wholesale.

All automated game tests must use an isolated profile and +set s_volume_dB -60. Preserve the player's profile and system volume.

Expose optional features through PreyConfigurator. Do not publish releases or change repository visibility without an explicit user request. Private source updates are the current workflow.

The runtime layout is engine/ for binaries and game data, userdata/ for saves and configuration, and Prey2006 Reawakened Launcher.exe at the package root. Do not introduce absolute developer paths.
