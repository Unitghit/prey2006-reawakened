// Native settings schema. Keep labels and defaults here for future features.
const std::vector<Setting>& Options() {
    static const std::vector<Setting> options = {
        {L"Display",L"Window mode",L"r_fullscreen",L"0",L"Windowed fullscreen is borderless and uses your desktop resolution.",{{L"Windowed",L"0"},{L"Fullscreen",L"1"},{L"Windowed fullscreen",L"desktop"}}},
        {L"Display",L"Resolution",L"resolution",L"1280x720",L"Windowed rendering follows the window size.",{{L"1280 x 720",L"1280x720"},{L"1920 x 1080",L"1920x1080"},{L"2560 x 1440",L"2560x1440"},{L"3840 x 2160",L"3840x2160"}}},
        {L"Display",L"Frame limit",L"com_maxFPS",L"-1",L"Match Display caps 3 FPS below the refresh rate of the monitor running the game, leaving headroom for variable refresh rate displays. VSync can impose a lower limit.",{{L"Match Display (default)",L"-1"},{L"60 FPS",L"60"},{L"120 FPS",L"120"},{L"144 FPS",L"144"},{L"165 FPS",L"165"},{L"240 FPS",L"240"},{L"Uncapped",L"0"}}},
        {L"Display",L"VSync",L"r_swapInterval",L"0",L"",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
        {L"Graphics",L"Anti-aliasing (MSAA)",L"r_multiSamples",L"4",L"Smooths geometry and texture cutout edges. Higher levels use more GPU memory and rendering time. Applies on next launch.",{{L"Off",L"0"},{L"2x",L"2"},{L"4x (default)",L"4"},{L"8x",L"8"}}},
        {L"Graphics",L"Bloom",L"bloom",L"enhanced",L"Corrected retail bloom. Enhanced follows internal resolution; Original uses a 256 x 256 blur.",{{L"Off",L"off"},{L"Original resolution",L"original"},{L"Enhanced (default)",L"enhanced"}}},
        {L"Graphics",L"Flashlight dynamic shadows",L"g_lighterShadows",L"1",L"Dynamic shadows from the lighter. Can reduce performance in complex scenes.",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
        {L"Graphics",L"Portal render distance",L"r_portalDistanceScale",L"8",L"Viewing distance relative to original. Portal nesting stays at three layers. Longer distances can reduce performance.",{{L"1x original",L"1"},{L"2x original",L"2"},{L"3x original",L"3"},{L"4x original",L"4"},{L"8x original (default)",L"8"},{L"16x original",L"16"}}},
        {L"Controls",L"Smooth motion",L"smoothMotion",L"1",L"Smooths camera, world objects, weapons, effects and portal transitions.",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
        {L"Controls",L"Xbox controller support",L"in_useGamepad",L"1",L"Keyboard and mouse remain available.",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
        {L"Controls",L"Invert controller look",L"joy_invertLook",L"0",L"",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
        {L"Controls",L"Controller dead zone",L"joy_deadZone",L"0.25",L"",{{L"10%",L"0.1"},{L"15%",L"0.15"},{L"25% (default)",L"0.25"}}},
        {L"Gameplay",L"Alien screen text",L"gui_translateAlienFontDistance",L"0",L"Translate nearby interactive screens when you approach them.",{{L"Untranslated (default)",L"0"},{L"English",L"200"}}},
        {L"Gameplay",L"Doom 3 weapons",L"weaponPack",L"off",L"Adds the Shotgun (slot 2), Machine Gun (slot 4), Chaingun (slot 5), Plasma Gun (slot 6), Rocket Launcher (slot 7), and Super Shotgun (slot 6, purple third weapon), unlocked with each slot's original weapon. Total ammo: 16 shells, 180 Machine Gun rounds, 180 Chaingun rounds, 150 Plasma cells, 12 Doom rockets, 16 Super Shotgun shells. Requires locally imported Doom 3 assets; Super Shotgun additionally requires Resurrection of Evil. Disabling preserves ammo and save progress; keep imported assets installed.",{{L"Disabled (default)",L"off"},{L"Enabled (prototype)",L"doom3shotgun"}}},
        {L"Gameplay",L"Bunny hopping",L"g_bunnyHop",L"0",L"Single-player. Quake: strafe and turn for speed. Painkiller-inspired: hold movement and jump to chain hops, with easy air steering and a 1.875x normal-speed cap. Original jump height.",{{L"Disabled (default)",L"0"},{L"Quake style",L"1"},{L"Painkiller style",L"2"}}},
        {L"Gameplay",L"Difficulty override",L"g_forceCherokee",L"0",L"Respect saved difficulty restores the mode stored in each save. Overrides apply when loading saves; subsequent saves record the chosen mode. Cherokee is always unlocked.",{{L"Respect saved difficulty (default)",L"0"},{L"Always Cherokee",L"1"},{L"Always Normal",L"2"}}},
        {L"Performance",L"Asset preloading",L"com_assetPreload",L"1",L"Experimental. Learns archive assets used during play and prepares them in the background on later visits. Uses up to 128 MB of cache. Applies on map or save load.",{{L"Enabled (experimental)",L"1"},{L"Disabled",L"0"}}},
        {L"Performance",L"Hitch logging",L"com_hitchTrace",L"1",L"Records slow CPU frames and asset operations in the game log to help investigate occasional stalls. Logging adds some overhead.",{{L"Enabled",L"1"},{L"Disabled",L"0"}}},
    };
    return options;
}
