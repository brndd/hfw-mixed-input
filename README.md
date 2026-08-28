# Horizon Forbidden West: Mixed Input Fix

A non-invasive, signature-based proxy DLL mod (`version.dll`) for *Horizon Forbidden West Complete Edition* on PC designed specifically for the **Steam Controller**.

## Features
- **Simultaneous Analog Locomotion + 1:1 Trackpad Mouse Look**: Full trackpad and gyro mouse precision without disabling analog stick movement or analog triggers.
- **HUD & Prompt Stability**: Prevents flickering between keyboard and gamepad button glyphs while maintaining native prompt rendering.
- **SIAPI Automatic Action Set & Layer Synchronization**:
  - Automatically switches the Right Trackpad to a **Radial Joystick** when holding <kbd>L1</kbd> (Weapon Wheel).
  - Automatically switches to the **`MenuControls`** Action Set when opening Map or Inventory menus.
  - Automatically restores **`InGameControls`** with 1:1 Raw Mouse Look during gameplay.
- **Future-Proof Pattern Scanner**: Uses wildcard AOB signatures that survive minor game patches.

---

## Installation

Copy `version.dll` into your *Horizon Forbidden West* game directory (where `HorizonForbiddenWest.exe` is located).

Launch the game through Steam. Check `mixed_input_fix.log` in the game directory to confirm initialization.

---

## Building from Source

Requires `mingw-w64`:

```bash
make clean && make
```
