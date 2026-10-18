Native Nintendo New 3DS XL driver station with a two-screen graphical interface, built-in controller input, and direct Wi-Fi UDP communication with the roboRIO.

- **3dsDS.cia:** install using FBI for a HOME Menu icon.
- **3dsDS.3dsx:** launch using Homebrew Launcher.
- **3dsDS-sd.zip:** extract to the SD card for the Homebrew Launcher layout.
- **SHA256SUMS:** checksums for the downloads.

Enter your team number in Setup; the roboRIO address is derived automatically. Drive uses dropdown selectors for mode and alliance. Inputs shows both sticks, all ten physical buttons, and a D-pad graphic with directional highlights.

Tap the large ENABLE button with neutral controls and a ready robot to request enable. While enabled, the entire lower screen turns red and becomes one DISABLE button. Tap anywhere to stop. HOME/suspend and connection loss clear enable, and resuming/reconnecting requires another tap. Physical button combinations do not change enable state.

Experimental release: host tests and build/package checks pass. Physical New 3DS XL behavior and an actual 2026 roboRIO still require secured bench testing. No FMS support. See the README for networking, mappings, and test instructions.
