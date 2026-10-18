Nintendo New 3DS XL HOME Menu app for FRC roboRIO robot control.

**Install:** download `3dsDS.cia`, copy it to the SD card, and install with FBI. Launch 3dsDS from HOME Menu.

Changes:
- Added Feed: a large, read-only live text box for roboRIO prints, warnings, errors, and stack traces.
- Text wraps and automatically scrolls to the latest messages. No editing or extra controls.
- Added an independent, nonblocking console connection with automatic reconnect.
- Navigation now has Drive, Inputs, Wi-Fi, and Feed. Enabled operation still shows only full-screen Disable.

Wi-Fi still detects security automatically, recommends only team networks found in a fresh scan, and remembers one last-used connection. The password field supports the native keyboard and Show/Hide.

Experimental release: automated protocol, safety, UI, and Wi-Fi service tests and CIA package checks pass. Physical console output, 3DS Wi-Fi, and compatibility with a 2026 roboRIO still need hardware testing. No FMS support.
