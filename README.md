# Prey2006 Reawakened

A remaster of Prey (2006) for modern Windows PCs, with a portal gun.

It runs at any frame rate, fixes the lighting and bloom, draws portals further and deeper, and fixes a long list of crashes and glitches. On top of that you can turn on a Portal-style portal gun, the Doom 3 weapons, and a few classic movement styles.

This is a fork of FriskTheFallenHuman's [Prey2006](https://github.com/FriskTheFallenHuman/Prey2006) source port, with fixes adapted from themuffinator's [openPREY](https://github.com/themuffinator/openPREY).

## Features

- Unlocked frame rate, with smooth motion for the camera, weapons, vehicles and portal crossings
- Corrected lighting, specular and bloom
- Longer portal draw distance and portals inside portals (up to six deep)
- Optional dynamic shadows for the flashlight and muzzle flashes
- Portal gun: works on Prey's walls, floors and ceilings, and carries you, enemies, ragdolls and projectiles through with momentum
- Doom 3 weapons alongside Prey's own (needs Doom 3)
- Movement styles from Quake, Painkiller, Half-Life and Half-Life 2
- Controller support (Xbox, PlayStation, Switch and most others) with button remapping in the launcher, English translation of the alien screens, Cherokee difficulty unlocked
- A launcher that finds your copy of Prey and sets everything up

The full list is in the [changelog](CHANGELOG.md).

## Installing

You need Prey (2006) for PC from Steam, GOG or disc, and 64-bit Windows 10 or 11. No game files are included.

1. Download the zip from [Releases](https://github.com/Unitghit/prey2006-reawakened/releases) and extract it anywhere.
2. Run `Prey2006 Reawakened Launcher.exe`.
3. Setup finds your games and installs. It never modifies your original install.
4. Choose your settings and hit Save & Play.

If you own Doom 3 or Portal, Setup can also convert the Doom 3 weapons, or the portal gun's model and sounds, from your copies. The portal gun works without Portal, just without a gun model.

Your saves, settings and log file are kept in the `userdata` folder. To update, extract the new version over the old one.

## Known issues

See [KNOWN-ISSUES.txt](docs/release/KNOWN-ISSUES.txt). Windows is the only tested platform; Linux and Proton haven't been tested yet.

## Building

See [BUILDING.md](BUILDING.md).

## Credits and license

Prey was made by Human Head Studios, on id Software's Doom 3 engine. Full credits are in [CREDITS.md](CREDITS.md).

The upstream license text is in [COPYING.txt](COPYING.txt). [LICENSING.md](docs/LICENSING.md) covers which license applies to which part, including the bundled libraries.
