# Changelog

## Unreleased

- Controller menu navigation: the D-pad moves between menu items, tabs and
  popup buttons, A selects, left/right change sliders and choices, and LB/RB
  switch between a menu's main tabs.
- In-game controller bindings show the same button names as the launcher (A button,
  LB, L stick up; Cross, L1 on PlayStation; ZL on Nintendo).
- Controllers missing from SDL's built-in list are recognised through the bundled
  SDL_GameControllerDB mappings.
- Controller window in the launcher (Controls > Configure controller...): shows the
  connected controller with its own button names (Xbox, PlayStation, Nintendo) and
  assigns each action by pressing a button on it. Shares the game's bindings.
- Controller menu navigation can be turned off in the launcher (Advanced).
- Console commands for comparisons: `dynamicShadows` toggles lighter and
  muzzle-flash shadows; `portalQuality` switches portals between retail Prey
  (original distances and recursion) and Reawakened.

## 1.0.1

- Fixed portals that could no longer be placed after a dead enemy's ragdoll
  fell partly through a floor portal.
- Xbox controllers work out of the box: a default controller layout is
  applied when no controller bindings are set. Your own bindings are kept.
- New launcher option: No fall damage (Gameplay, single-player).

## 1.0.0

First release of Prey2006 Reawakened, built on the Prey2006 source port with
fixes adapted from openPREY.

### Setup and launcher
- Guided Setup on first launch: finds Prey (2006) automatically (Steam, GOG,
  retail, moved drives) and imports its data archives without changing the
  installed game.
- Optional content converted from your own games with the bundled importer:
  Doom 3 weapons (original, Resurrection of Evil or BFG Edition) and the Portal
  gun's model and sounds. Add or remove them any time with "Import games...".
- Native settings launcher with descriptions, an Advanced view and Save & Play.
- Works from long, spaced and non-English folder paths, and explains when a
  folder cannot be used (too long, or not writable such as Program Files).

### Frame rate and presentation
- Unlocked frame rate with a display-matched cap and VSync option.
- Smooth camera, weapons, objects, effects, vehicles, scripted rides and portal
  crossings between 60 Hz game ticks.
- Faster level loads: threaded texture and sound decoding, fewer loading holds.
- Save files written in the background.

### Rendering
- Corrected retail lighting, specular and weapon rendering.
- Reconstructed, resolution-scaled bloom (enhanced or original).
- Longer portal render distance, nested portal views with up to six layers,
  and skyboxes, bloom and lighting through portals.
- Optional dynamic flashlight shadows and muzzle flash shadows.
- MSAA with smoothed alpha-tested edges; windowed fullscreen.
- Fixes for decal flicker, portal glow flicker, eye textures, Jen's chest seam
  and many portal rendering problems.

### Portal gun
- Slot-1 portal gun (press 1 again to switch from the wrench): blue and orange
  portals on stationary surfaces, with momentum preservation.
- Players, NPCs, ragdolls, objects and projectiles travel through portals,
  including floor, wall and ceiling combinations.
- Placement crosshair showing where each portal can go.
- Portal gun model, animations and sounds from Portal (optional import).

### Doom 3 weapons (optional)
- Shotgun, Machine Gun, Chaingun, Plasma Gun, Rocket Launcher, BFG 9000 and
  Super Shotgun, grouped with Prey's weapons and unlocked with each slot's
  original weapon, with their own ammunition.
- Save compatible: disabling them keeps your progress.

### Gameplay
- Movement styles: Original, Quake, Painkiller, Half-Life 1 and Half-Life 2
  (checked against Valve's movement code), with optional automatic jumping.
- Adaptive Hard and Cherokee difficulties and a difficulty override.
- Optional retry from checkpoint instead of spirit resurrection.
- Optional English translation of alien screen text.
- On-screen messages when the portal gun and Doom 3 weapons unlock.
- Xbox controller support alongside keyboard and mouse.

### Fixes
- Crashes on some loads and saves, and in scripted scenes.
- Stuck or trapped player cases around portals, rails and floors.
- Streamed sounds (the Roadhouse jukebox) staying in sync.
- In-world screen interaction and cursor issues.
- Saves, settings and the game log always stay in the installation's userdata
  folder, including when prey06.exe is started directly.
