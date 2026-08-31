# Horizon Forbidden West: Decima Engine Input Architecture

## Multi-Tier Input Pipeline & Mutual Exclusion
Decima implements dual-device mutual exclusion across three separate tiers:

### Tier 1: Master Device Samplers (`NxInputImpl`)
- **`NxInputImpl::GetAnalogValue` (RVA `0x8C110`)**:
  - Evaluates control IDs against `ActiveDevice` (1 = KBM, 2 = Gamepad).
  - Control IDs 500..510 (Mouse axes/deltas) are zeroed if `ActiveDevice == 2`.
  - Control IDs 600..610 (Gamepad sticks/triggers) are zeroed if `ActiveDevice == 1`.
  - Unblocked by NOPing the device comparison branches at `+0x8C16E`, `+0x8C1CF`, `+0x8C21D`, `+0x8C281`, `+0x8C2B1`.
- **`NxInputImpl::GetDigitalState` (RVA `0x8BF00`)**:
  - Unblocked by NOPing the comparison branches at `+0x8BF56`, `+0x8BFC1`, `+0x8C022`, `+0x8C09A`, `+0x8C0CD`.
- **`NxInputImpl::ProcessRawMouseInput` (RVA `0x8AF20`)**:
  - Ingests Windows `WM_INPUT` mouse deltas into `NxInputImpl + 0x4fc` (X) and `+ 0x500` (Y).
  - Flushed to `0.0f` at the end of each frame in `NxInputImpl::Tick` (`0x8B910`).

### Tier 2: Action Context & Device Switching
- **`NxInputImpl::ProcessCursorTick` (RVA `0x8A950`)**:
  - On mouse movement, calls `NxInputImpl::SetActiveDevice` (`0x86830`) at `0x8AADF`, setting `ActiveDevice = 1`.
  - `NxInputImpl::UpdateGamepadActiveContext` (`0x86790`) disables the `"GamepadActive"` action context in `NxActionManager`.
  - When `"GamepadActive"` is disabled, player locomotion (`MoveStick`) is deactivated, causing stuttering/pauses during simultaneous mouse input.
  - Resolved by NOPing the `call SetActiveDevice` at `+0x8AADF`.

### Tier 3: Camera Component Routing (`ThirdPersonPlayerCameraComponent`)
- **`ThirdPersonPlayerCameraComponent::SampleInput` (RVA `0x118EEA0`)**:
  - Stores Gamepad stick look delta at `component + 0x130` and Mouse look delta at `component + 0x140`.
- **`ThirdPersonPlayerCameraComponent::CalculateLookRotation` (RVA `0x1193830`)**:
  - At `0x1193CE7`, a conditional branch (`je 0x1193CFF`) selects Gamepad look branch if `"GamepadActive"` is enabled, discarding the mouse look delta.
  - **Key Lesson**: Do not inject custom vector math into component stack frames (causes register clobbering / uninitialized stack reads and infinite camera spinning).
  - **Resolution**: NOP the 2-byte branch (`74 16` -> `90 90` at `+0x1193CE7`) so the camera component always executes the engine's native, compiled Mouse Look calculation while the rest of the game stays in Gamepad mode.

### Tier 4: Photo Mode & FreeCamera Architecture (`PhotoModeMenuController`)
- **`PhotoModeMenuController::UpdateFreeCamera` (RVA `0x13DC2C0`)**:
  - Handles spatial camera translation (`WASD` / Left Stick) via `PhotoModeMenuController + 0x30` -> `FUN_141187bc0`.
  - Dispatches FreeCamera orientation updates via `ThirdPersonPlayerCameraComponent::Update` (`0x118DFE0`).
  - Evaluates `[param_1 + 0x1e8]` (active device) to switch between gamepad stick look (`component + 0x130`) and mouse look capture (`SetMouseCapture`).
- **`ThirdPersonPlayerCameraComponent::CalculateFreeCameraLookRotation` (RVA `0x1190230`)**:
  - Calculates FreeCamera rotation using `component[0x140] * component[0x180]` (Mouse Look delta * sensitivity) and `component + 0x130` (Gamepad Stick Look).
  - Hooked to inject SIAPI trackpad deltas into `component + 0x140`.
- **Photo Mode Sensitivity Scaling Architecture**:
  - In vanilla Decima, `SampleInputLookState` (`0x14118B260`) calculates Photo Mode sensitivity as `gameplay_sensitivity * (FOV_scale * 40.0f)` into `component + 0x180`.
  - Because vanilla Decima only runs `SampleInputLookState` when RMB is clicked/held (`m_MouseCaptured == 1`), entering Photo Mode leaves `component + 0x180` at raw gameplay sensitivity (`< 0.001f`) until RMB is clicked.
  - In `ProcessFreeCamSampleInputLook`, when `sensitivity[0] < 0.001f`, we scale trackpad deltas by `(FOV_scale * 40.0f)` before adding to `component + 0x140`.
  - This ensures trackpad sensitivity in Photo Mode is 100% consistent from the very first frame without requiring RMB to be clicked, while leaving physical mouse cursor and RMB capture under native Decima control.

---

## Executive Summary: Photo Mode Mixed Input Architecture & Resolution

### 1. Root Cause & Architectural Insight
- **Decima Photo Mode Pause State Machine**:
  - In `ThirdPersonPlayerCameraComponent::SampleInputLookState` (`0x14118B260`), when the game is paused from Gamepad mode, Decima detects `[camera + 0x500]->state == 3` (PAUSED) early in the function and returns immediately, skipping camera-level mouse ingestion.
- **Resolution via Native Raw Mouse Ingestion**:
  - `NxInputImpl::ProcessRawMouseInput` (`0x14008AF20`) continuously ingests Windows `WM_INPUT` mouse deltas into `NxInputImpl + 0x4fc` (X) and `+ 0x500` (Y) on every frame regardless of game pause state.
  - We capture `g_pNxInputImpl` via a minimal 15-byte detour at `0x14008AF20`.
  - In `ProcessFreeCamSampleInputLook` (`CalculateFreeCameraLookRotation` at `0x141190230`):
    - **When RMB is NOT held**: Flushes `mouse_buffer[0..3]` to zero (preventing UI cursor drift / SIMD corruption) and ingests SIAPI trackpad deltas scaled by `(FOV_scale * 40.0f)`.
    - **When RMB IS held**: If Decima's camera buffer is zero (due to entering pause from gamepad), reads `NxInputImpl + 0x4fc` and `+ 0x500` directly and scales them by the Photo Mode FOV multiplier. SIAPI trackpad deltas are mixed cleanly on top.

### 2. Validated Architecture Summary
- **SIAPI Trackpad FreeCamera Panning**: Active with or without RMB held; scaled by `(FOV_scale * 40.0f)` when RMB is not held and handled by native sensitivity when RMB is held.
- **Physical Mouse Cursor & Pan**: Decima natively handles UI cursor navigation when RMB is released and FreeCamera panning when RMB is held (reading directly from Decima's `NxInputImpl` raw mouse buffer).
- **Simultaneous Gamepad Support**: Gamepad sticks, buttons, and locomotion function seamlessly alongside trackpad look and mouse input.
- **Minimal Footprint**: Built with modern C++23 and CMake using SafetyHook (Zydis AVX2-aware disassembler). Operates with 3 clean inline hooks (`SampleInputLookState`, `CalculateFreeCameraLookRotation`, `ProcessRawMouseInput`), 2 context hooks (`EnableContext`, `DisableContext`), and 1 NOP branch in `CalculateLookRotation`. No naked assembly thunks, no manual register preservation, and no OS cursor hacks.




