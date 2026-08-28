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
