Prey2006 Reawakened
===================

An enhanced version of Prey (2006) for Windows: unlocked frame rate with smooth
motion, corrected lighting and bloom, deeper portal rendering, a portal gun,
optional Doom 3 weapons, Half-Life style movement, and many fixes.

Built on FriskTheFallenHuman's Prey2006 source port, with fixes adapted from
themuffinator's openPREY. Full credits are in THIRD-PARTY-NOTICES.txt.

You need your own copy of Prey (2006) for PC (Steam, GOG or retail disc).
No Prey, Doom 3 or Portal game files are included in this download.


Requirements
------------
- Windows 10 or 11, 64-bit.
- Prey (2006) for PC, installed.
- Optional: Doom 3 (original or BFG Edition) for the Doom 3 weapons.
  Resurrection of Evil adds the Super Shotgun.
- Optional: Portal for the portal gun's model and sounds. The portal gun works
  without it, shown without a gun model.


Installing
----------
1. Extract this folder anywhere you like, for example C:\Games.
   Keep the folder path reasonably short (under about 170 characters).
2. Run "Prey2006 Reawakened Launcher.exe".
3. Setup opens on the first launch. It finds your games automatically; if a
   game is not found, click Browse and select its installation folder.
4. Click Install. Setup copies Prey's data archives into this folder (on the
   same drive it links them instead, using no extra space) and converts any
   optional games you ticked. Your installed games are never changed.
5. Choose your settings in the launcher and click Save & Play.

Nothing is written outside this folder except %TEMP%\Reawakened-import,
where Setup converts content and keeps its logs. You can delete it any time.


The launcher
------------
- Save & Play saves your settings and starts the game.
- "Import games..." adds or removes the optional Doom 3 and Portal content.
- Tick "Advanced" to show more options (shadows, bloom, portal rendering,
  portal crosshair, Half-Life automatic jumping, unlock messages).
- Hover over any option for a description.


Where your files are
--------------------
- Settings and saves: the userdata folder in this installation.
- Prey's data: engine\base (pak000-pak006.pk4, from your copy of Prey).
To back up your progress, copy the userdata folder.


Updating
--------
Extract the new version over this folder and replace files when asked. Your
userdata folder (settings and saves) and imported game data are kept. If the
launcher opens Setup, click Install to rebuild any files the update needs.


Uninstalling
------------
Delete this folder. Nothing else is installed; your games are untouched.


Troubleshooting
---------------
- "This is not a Prey (2006) installation": select the folder that contains
  the base folder (for example ...\steamapps\common\Prey), not base itself.
- The game will not start from this folder: move it to a short path with plain
  letters, such as C:\Games\Prey2006 Reawakened.
- Setup keeps a log of each conversion in %TEMP%\Reawakened-import.
- Saves made while the Doom 3 weapons or portal gun were in use need that
  content to load. Add it again with "Import games..." (from any copy of the
  game) and those saves work as before.


More information
----------------
CHANGELOG.txt           What is in this version.
KNOWN-ISSUES.txt        Known problems.
THIRD-PARTY-NOTICES.txt Credits and licenses; license texts are in licenses\.
Source code:            see the project page this download came from.

Prey (2006) is a trademark of its respective owners. This is an unofficial,
non-commercial fan project and is not affiliated with or endorsed by them.
