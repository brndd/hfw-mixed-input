# Horizon Forbidden West: Mixed Input Fix

Vibe reverse engineered and vibecoded mod to patch mixed input support into Horizon Forbidden West Complete Edition (Steam version). Enables the game to receive simultaneous controller and mouse input through Steam Input without shitting its pants. Intended for use with the Steam Controller and other such touchpad controllers.

## Installation

Copy the following files into your game directory (where `HorizonForbiddenWest.exe` is located):
- `version.dll`
- `mixed_input_fix.ini`
- `steam_input_manifest.vdf`
- `steam_input_steamcontroller.vdf`

Launch the game through Steam. Check `mixed_input_fix.log` in the game directory to confirm initialization (or just, you know, try it). Set in-game mouse sensitivity to 1.0 on all axis for best results with the built-in profile.

If you're on Linux, set the game's launch options to `WINEDLLOVERRIDES="version=n,b" %command%` or else the DLL won't load. Yes you can run Gamescope and whatever other bells and whistles with this, just stick the WINEDLLOVERRIDES part at the very beginning of the command.

## Configuration & Input Modes

The mod's behavior can be configured via `mixed_input_fix.ini`:

```ini
[General]
mode = siapi
```

- **`siapi`** *(Default & Recommended)*: Direct Steam Input API integration using native memory and camera hooks. Ingests trackpad deltas directly into camera components, handles automatic action set switching (InGame, Weapon Wheel layer, Menu), and supports full Photo Mode FreeCamera panning with both the trackpad and physical mouse (RMB held).
- **`raw_mouse`** *(Legacy)*: Unblocks Decima's internal input samplers in `NxInputImpl` via 12 binary memory patches. Default to `siapi`, but switch to `raw_mouse` if `siapi` breaks on a newer game patch or if you really need regular physical mouse input simultaneously with gamepad locomotion for your specific usecase. In `raw_mouse` mode, Photo Mode stays on `MenuControls`.

## Game version support

This worked when I built it, with whatever game version was the newest at that time. If it doesn't work anymore make an issue and I might see if it can be fixed. The patches aren't super robust against binary changes from updates so this is liable to break.

This is completely built around Steam Input. No it will not work with the EGS version, no it will not work with your pirated copy unless Steam emulators have massively stepped up their Steam Input emulation game since I last looked. Yes a version could theoretically be made that works with those, no I will not make it.

## Controller support

Currently a Steam Input controller profile is provided only for the first gen Steam Controller, because I don't own a Steam Deck or Steam Controller (2026). Pull requests and device donations welcome.

The controller profile needs to have the following actionset IDs:

- `InGameControls` **actionset** for normal gameplay controls
- `WeaponWheelControls` actionset **layer** as a child of `InGameControls` to turn right trackpad into analog stick for the weapon wheel
- `MenuControls` **actionset** to turn right trackpad into analog stick for menus.

The mod is hardcoded for these IDs and has hooks in the game's context manager to change the controller actionset.

## Building from Source

Requires CMake 3.25+ and `mingw-w64` C++ toolchain:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Or build distributable zip package:
cmake --build build --target package_zip
```

The compiled output binary is located at `build/version.dll` and the distributable zip at `build/hfw-mixed-input-*.zip`.

## Slop warning

This is slop. I pointed Antigravity (running Gemini 3.7 Flash, yes I know it's the worst one) at Ghidra, told it to figure things out and tested its patches in game until it got one right with only minor guidance after 3 or 4 attempts. If you don't like that, then rest assured that (1) I don't like it either, and (2) I am someone who [could have done this himself](https://github.com/brndd/vanilla-tweaks/) but didn't feel like wasting 2-3 days poking around an unfamiliar binary when I just wanted to play the damn game.
