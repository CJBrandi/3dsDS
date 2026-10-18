Nintendo New 3DS XL HOME Menu app for FRC roboRIO robot control.

**Install:** download `3dsDS.cia`, copy it to the SD card, and install with FBI. Open 3dsDS from HOME Menu. This release is distributed as a CIA only.

Changes:
- Added an in-app Wi-Fi page with a team-based SSID suggestion, editable network name, masked password keyboard, WPA2/Open selector, saved-slot selector, and Save & connect.
- Connection runs asynchronously with timeout/cancel handling. The robot remains disabled while changing Wi-Fi. The selected system Wi-Fi slot is replaced; its current network name is shown before saving. Passwords are stored in the console's network profile, not the app's SD config or logs.
- Centered Disable text using the actual rendered font dimensions.
- Removed Quit to launcher and the Inputs screen's touch-control hint.

`FRC-<team>` is a suggestion for the robot radio, not a detected network. A separate access-point radio may use `FRC-AP-<team>` with a suffix. Enter the exact SSID shown by your radio. Use a 2.4 GHz WPA2-capable network; WPA3-only is unsupported by the 3DS.

Experimental release: protocol/safety/UI tests, mocked Wi-Fi service tests, and build/package checks pass. Actual Wi-Fi profile writes, network association on a physical New 3DS XL, and 2026 roboRIO operation still need hardware validation. No FMS support.
