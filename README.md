# Horizon Forbidden West: Mixed Input Fix

Vibe reverse engineered and vibecoded mod to patch mixed input support into Horizon Forbidden West Complete Edition (Steam version). Enables the game to receive simultaneous controller and mouse input through Steam Input without shitting its pants.

Intended primarily for use with the Steam Controller and other such touchpad controllers. It can also be useful to get better gyro aiming on other controllers.

## Installation

Download the mod from the Releases page [or click here to download the latest release](https://github.com/brndd/hfw-mixed-input/releases/latest/download/hfw-mixed-input-v0.8.zip).

Copy the contents of the .zip into your game directory (where `HorizonForbiddenWest.exe` is located).

Launch the game through Steam. Check `mixed_input_fix.log` in the game directory to confirm the patch works (or just, you know, try it).

**Set in-game mouse sensitivity to 1.0 on both axis for best results with the built-in profile.** Due to the way we override the default profile/controller manifest, the modified default profile with the custom ActionSets might only be visible while the game is running.

If you're on Linux, set the game's launch options to `WINEDLLOVERRIDES="version=n,b" %command%` or else the DLL won't load. Yes you can run Gamescope and whatever other bells and whistles with this, just stick the WINEDLLOVERRIDES part at the very beginning of the command.

## Configuration

The mod's behavior can be configured via `mixed_input_fix.ini`:

```ini
[General]
mode = siapi
disable_mouse_smoothing = false
```

- **`mode`**:
  - **`siapi`** *(Recommended)*: Direct Steam Input API integration using a custom SteamTouchPad control, similar to Horizon Zero Dawn Remastered. This has the advantage of stopping the cursor from wandering off-screen during cutscenes and dialogue that don't lock it, plus it has proper support for Photo Mode.
  - **`raw_mouse`** *(Legacy)*: Unblocks simultaneous raw mouse & controller input. This is useful if you for some reason want to use your actual mouse together with a controller, or if you want to keep the Steam Controller touchpad in raw mouse mode, or if the SIAPI patch just doesn't work for some reason. The downside is poor Photo Mode support and a few more edge cases.
    - **Note:** if using raw_mouse with a Steam Controller, the provided controller profile won't work for you and you must change the controller settings to put the touchpad and gyro in mouse mode yourself.
- **`disable_mouse_smoothing`**: The game has a slight unconfigurable mouse smoothing/filtering effect. This allows disabling it.
  - **`false`** *(Default)*: Mouse smoothing remains active.
  - **`true`**: Mouse smoothing is disabled.

## Game version support

This was built against the 2024-06-27 Steam build of the game. The patches aren't super robust against binary changes from updates so this is liable to break. If it doesn't work anymore make an issue and I might see if it can be fixed.

This is completely built around Steam Input. No it will not work with the EGS version, no it will not work with your pirated copy unless Steam emulators have massively stepped up their Steam Input emulation game since I last looked. Yes a version could theoretically be made that works with those, no I will not make it.

## Controller support

Profiles for all major controllers are bundled. However, only the Steam Controller and PS4 profiles have been tested; the rest are experimental. Pull requests and device donations welcome.

To add your own profile locally, you can edit `steam_input_manifest.vdf` and configure a new `steam_input_<controller>.vdf` near the top, then put your actual controller config in that file. Look at the existing ones in `assets/` for examples. You can also use Steam's controller configuration GUI to create these, but the controls may not appear right there if the manifest doesn't already provide a config for your controller.

The controller profile needs to have the following ActionSet IDs:

- `InGameControls` **ActionSet** for normal gameplay controls
- `WeaponWheelControls` ActionSet **layer** as a child of `InGameControls` to turn right trackpad into analog stick for the weapon wheel
- `MenuControls` **ActionSet** to turn right trackpad into analog stick for menus.

The mod is hardcoded for these IDs and has hooks in the game's context manager to change the controller actionset.

## Building from source

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
