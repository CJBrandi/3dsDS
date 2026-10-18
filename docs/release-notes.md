Nintendo New 3DS XL HOME Menu app for FRC roboRIO robot control.

**Install:** download `3dsDS.cia`, copy it to the SD card, and install with FBI. Launch 3dsDS from HOME Menu.

Changes:
- Merged Drive and Setup into one Drive page: alliance station, Team Number, then Enable.
- The team field contains only digits and opens the numeric 3DS keyboard.
- Restricted the app and outgoing control packets to Teleop. Removed the mode selector.
- Simplified navigation to Drive, Inputs, and Wi-Fi, and refreshed the screen renders.

Wi-Fi still detects security automatically, recommends only team networks found in a fresh scan, and remembers one last-used connection. The password field supports the native keyboard and Show/Hide.

Experimental release: automated protocol, safety, UI, and Wi-Fi service tests and CIA package checks pass. Physical 3DS Wi-Fi and compatibility with a 2026 roboRIO still need hardware testing. No FMS support.
