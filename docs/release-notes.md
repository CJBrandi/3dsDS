Native Nintendo New 3DS XL driver station with a two-screen graphical interface, built-in controller input, and direct Wi-Fi UDP communication with the roboRIO.

- **3dsDS.cia:** install using FBI for a HOME Menu icon.
- **3dsDS.3dsx:** launch using Homebrew Launcher.
- **3dsDS-sd.zip:** extract to the SD card for the Homebrew Launcher layout.
- **SHA256SUMS:** checksums for the downloads.

Drive, Setup, and Inputs tabs provide mode/alliance selection, team/IP configuration, live controller diagnostics, battery voltage, and robot status. Hold ZL + START for one second with neutral inputs to request enable; keep ZL held. Releasing ZL disables. Disable and E-stop touch buttons remain available while enabled.

Experimental release: host tests and build/package checks pass. Physical New 3DS XL behavior and an actual 2026 roboRIO still require secured bench testing. No FMS support. See the README for networking, mappings, and test instructions.
