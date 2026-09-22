# Gameplay menu cleanup

Removed the duplicate crosshair selector from Prey Settings and its saved options/custom command line. The game profile's crosshair choice is preserved.

Registered the previously missing com_profanity CVar with archived default 1 (On) and set the current play profile to 1. No launch-time override forces it back after later menu changes. The port currently plays unfiltered dialogue regardless of this preference; this repairs the misleading default menu value, not a missing censorship implementation.

Dynamic difficulty was inspected, not changed. game_dda.cpp recalculates difficulty from estimates for the enemy types currently attacking. Survival results can increase or decrease those estimates, and monster-caused death lowers the corresponding difficulty by 0.075. Disabled DDA fixes the aggregate at 0.5; g_wicked overrides this to 1.0. game_player.cpp multiplies incoming single-player damage by twice this value. Enemy-specific code also varies dodge probabilities, delays and other behavior.

Validation: Release build and configurator publish succeeded. Muted isolated startup reports com_profanity current/default 1. Configurator serialization and all existing layout checks passed. Deployment is validation/gameplay-defaults-build via Prey Settings/custom launcher.
