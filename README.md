# 3dsDS

A native Nintendo New 3DS XL homebrew driver station for a roboRIO running FRC robot code. Uses the built-in controls and the console's 2.4 GHz Wi-Fi. No PC bridge, Python, external controller, or extra runtime packages are needed on the 3DS. Written in C using devkitARM/libctru, statically linked Citro2D/Citro3D graphics, and Mbed TLS crypto for WPA2 password derivation.

**Status:** builds into an installable HOME Menu `.cia`; protocol, safety, UI, and mocked Wi-Fi service tests pass on Linux. Physical 3DS Wi-Fi/input behavior and compatibility with an actual **2026 roboRIO image have not yet been tested**. This implements the standalone UDP control protocol from the supplied `nexus.py`, with protocol corrections. It is an experimental practice/bench driver station, with no FMS integration or competition certification claimed.

![Interface layout preview](docs/ui-preview.png)

[Setup, Inputs, full-screen Disable, and Wi-Fi previews](docs/ui-states.png)

[All closed screens](docs/renders/screens-closed.png) · [All open selectors](docs/renders/selectors-open.png)

The preview is generated from the same UI layout code with a host SVG renderer. It illustrates the design; the console uses the 3DS system font and has not yet been photographed running this build.

## Install

Download the install files from [GitHub Releases](https://github.com/CJBrandi/3dsDS/releases).

Copy `3dsDS.cia` to the SD card, open FBI, select the file, and choose Install. Launch 3dsDS from HOME Menu. This app is distributed as a CIA only and has no Quit-to-launcher button. Use HOME to leave it.

The experimental CIA Title ID is `000400000F3D5200`, with Unique ID `0xF3D52`. It is project-assigned, not registered with Nintendo or reserved by Universal-DB. Installing a newer CIA with this ID updates the app.

## Run

1. Install `3dsDS.cia` with FBI and launch it from HOME Menu.
2. Open **Setup** and enter your team number, then open **Wi-Fi** to configure a 2.4 GHz network that reaches the robot. The app configures the console's connection; it does not configure the radio itself.
3. On **Wi-Fi**, wait for the scan, then tap the recommended **Join** button or pick a detected network. Enter its password in the native 3DS keyboard and tap **Join selected network**.
4. Open **Setup** and tap **TEAM NUMBER**. Enter your team and the app derives `10.TE.AM.2` (team 1234 → `10.12.34.2`). There is no IP entry or mDNS dependency. The robot must use that standard team address, and the 3DS must have a route to it.
5. Wait for **CONNECTED** and robot code **Running**. Verify battery and control readings. The top **ENABLE REQUESTED** label means the DS is requesting enable; **ROBOT REPORTS** is the robot's reported state.
6. With the robot secured and wheels raised for the first test, center both sticks and release all physical buttons/D-pad. On **Drive**, tap the large **ENABLE** button.
7. The entire lower screen turns red with **DISABLE**. Tap anywhere on it to disable the robot.

The app starts disabled in Teleop every launch. Team number and alliance station persist at `/3ds/3dsDS/config.txt`. The lower screen has Drive, Setup, Inputs, and Wi-Fi tabs while disabled. Drive uses dropdown selectors for mode and alliance. Setup accepts only the team number; the automatically derived address is read-only. Inputs shows two stick plots, ten physical buttons, and a D-pad graphic that highlights directions and diagonals. While enabled there are no tabs or configuration controls on the lower screen: every pixel is the Disable touch target.

If a firewall separates the devices, allow 3DS → roboRIO UDP 1110 and roboRIO → 3DS UDP 1150.

## Driver controls

| Control | Action |
| --- | --- |
| Large ENABLE button on Drive | Enable after a fresh reply, running robot code, and neutral inputs |
| Entire red lower screen while enabled | Tap anywhere to disable |
| HOME | Disable before suspension; returning stays disabled |
| Connection loss | Clear enable; reconnecting stays disabled |
| Mode selector while disabled | Teleop, Test, or Autonomous |
| Alliance selector while disabled | Red 1–3 or Blue 1–3 |

There are no physical button enable/disable bindings, button combos, or hold-to-run controls. START, SELECT, ZL, and ZR are robot inputs along with the face and shoulder buttons. A new touch-down is required for each action, so the same touch that enables cannot immediately disable, and a held touch cannot re-enable after a fault. Dropdown dismissal does not activate a button underneath it.

The robot's reported E-stop is honored and latched for the app session. Reset an E-stop at the robot and restart the app. This UI has no manual E-stop action; the red screen requests normal Disable.

Communication loss at 250 ms, missing robot code, socket failures, and a main-loop stall over 100 ms clear enable. HOME/sleep hooks send a disabled packet before suspension when networking is still available. While suspended the app cannot transmit; the roboRIO communication watchdog provides the fallback. On resume reply history is cleared. Reconnection, code recovery, and HOME return never automatically enable; tap ENABLE again with neutral inputs.

## In-app Wi-Fi setup

The Wi-Fi tab scans nearby 2.4 GHz access points and detects security from their advertised beacon information. **Join Recommended Network: [name]** and a large **Join** button appear only when a compatible matching team network is present in a scan less than 30 seconds old. Names such as `FRC-1234` and `FRC-AP-1234-A` match team 1234; `FRC-12345` does not. Tap **Scan** to refresh. Hidden SSIDs are not supported.

The network selector lists actual scan results with detected security. Selecting a WPA2 network opens the native 3DS password keyboard; selecting an open network needs no password. Tap the password field to edit it again, and the small **Show/Hide** button to reveal or mask the password entered this session. Saved system passwords are not read back into the app. WPA3-only, enterprise, and legacy security are labelled unsupported and cannot be joined.

The app remembers **one last-used connection**, reusing one console system Wi-Fi profile. Joining a different network replaces that remembered profile; there is no slot selector or multi-network manager. On first use it chooses an empty system profile if possible, otherwise the third profile. Passwords persist in the console's network settings; only team, alliance, the profile identifier, and detected security are saved to the app's SD config. Session password memory is cleared on exit or when selecting a different network. No passwords are logged.

Connections run asynchronously, can be cancelled, and time out. Robot enable is blocked during scanning or connection changes. HOME or sleep cancels the connection attempt and discards scan results. The app does not require an Internet connection: robot radios can be local networks.

The VH-109 [firmware changelog](https://frc-radio.vivid-hosting.net/advanced-topics/firmware-changelog) documents WPA2/3 transitional compatibility in version 2.0.1; 2.0.0 used WPA3-only on 2.4 GHz. Use the WPA2-compatible mode for 3DS.

Scanning follows the [NWM infrastructure request](https://docs.mikage.app/NWMINFRecvBeaconBroadcastData/) and [beacon reply layout](https://docs.mikage.app/NWM_Services/#beacondatareply-structure). Profile editing follows the [AC service documentation](https://www.3dbrew.org/wiki/AC_Services). These calls compile and pass service-mock tests; **physical 3DS scanning, profile writes, and association still require hardware validation**. Scan errors are shown explicitly and do not generate guessed recommendations.

## Robot-side controller mapping

The built-in controller is **joystick port 0**, a generic gamepad; it does not impersonate an Xbox controller. Use `Joystick` or `GenericHID`, and read raw axes/buttons. There are four axes, ten buttons, and one POV. Axis range is approximately -1 to +1 as exposed by WPILib; up is negative Y. An approximately 10% center deadzone is applied and the remaining range is rescaled.

| WPILib input | 3DS control |
| --- | --- |
| Raw axis 0 | Circle Pad X |
| Raw axis 1 | Circle Pad Y |
| Raw axis 2 | C-stick X |
| Raw axis 3 | C-stick Y |
| Raw buttons 1, 2, 3, 4 | A, B, X, Y |
| Raw buttons 5, 6, 7 | L, R, ZR |
| Raw buttons 8, 9, 10 | ZL, SELECT, START |
| POV 0 | D-pad; clockwise degrees from up, -1 when released |

Only the touch screen and system HOME button are used for driver station controls. Existing robot code that expects an `XboxController` must be updated to this mapping. No rumble hardware is available.

Example Java input reads (integrate into your existing drive subsystem):

```java
private final Joystick driver = new Joystick(0);
// Circle Pad forward and sideways, C-stick turn:
double forward = -driver.getRawAxis(1);
double sideways = driver.getRawAxis(0);
double turn = driver.getRawAxis(2);
boolean action = driver.getRawButton(1); // A
```

## Protocol scope

- Standalone UDP control to port 1110, reply socket bound to 1150, nominal 20 ms sends.
- Version 1, wrapping 16-bit sequence, mode/enable/E-stop, alliance, one joystick, UTC date/timezone handshake tags.
- Robot-code bit from trace byte 4 (`0x20`); battery from bytes 5–6 with a `/256` fraction. These correct the attached Python implementation's telemetry parsing.
- Replies must come from the configured robot IPv4 address, have a valid header/tag layout, echo a recently sent sequence, and advance the response sequence. Duplicates, old replies, and malformed packets cannot refresh link health. This is freshness checking, not cryptographic authentication.
- Date tags are resent while disconnected or requested by the robot. The calendar uses the console's clock as UTC; set that clock correctly if robot timestamps matter.
- No TCP joystick descriptors, robot console/error stream, NetworkTables/dashboard, code deployment, reboot commands, FMS, or automatic match timing. UDP joystick values work independently of those features in the reference protocol, but name/type metadata queries may return defaults.

## Build and test

With devkitPro's `3ds-dev` and `3ds-mbedtls` packages installed:

```sh
export DEVKITPRO=/opt/devkitpro
make -j2       # ELF + icon used for the CIA
make cia-tools # download pinned, checksum-verified Linux x86_64 packaging tools
make release   # HOME Menu CIA + notices + verified headers/checksums
make test       # host C compiler + libmbedtls-dev; address/undefined sanitizers
make preview   # host SVG layout preview
```

`make test` requires a host C compiler and `libmbedtls-dev`, but not the 3DS SDK. Wi-Fi tests use a fake AC service and a known WPA2 PSK vector to check profile staging, save failures, reconnect, timeout, cancellation, and key/password handling. It also checks every lower-screen pixel on every page/popup state maps to Disable while enabled, dropdown isolation, and team-only setup. It exercises exact packet vectors, date tags, telemetry offsets, malformed/truncated replies, all POV combinations, deadzones, touch enable/disable, each disabling condition, recovery without automatic enable, E-stop latching, duplicate/outdated replies, and sequence rollover. `make release` outputs `dist/3dsDS.cia`, `THIRD_PARTY_NOTICES.txt`, and `SHA256SUMS`; `make clean` removes generated outputs.

For this workspace the SDK was extracted from the official `devkitpro/devkitarm` container into a local cache. Rebuild here with:

```sh
make DEVKITPRO=/home/cj-brandi/.cache/3dsds-sdk/opt/devkitpro -j2
```

The container used was `devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`. The SDK is a build-time dependency, not bundled into the SD package.

### Bench check without robot hardware

On a Linux PC on the same reachable network:

```sh
python3 tools/mock_robot.py --bind 0.0.0.0
```

Because the app accepts only team numbers, the mock PC must have the team's derived address (for example `10.12.34.2` for team 1234) on a dedicated test network that does not contain a real robot. Do not assign that address if it would conflict with an existing roboRIO. The mock prints all controls and reports 12.50 V and running code. Verify touch enable, the full-screen Disable target, and physical button independence. Repeat with `--drop-after 10` for link loss and `--no-code` for the code interlock. This is not a substitute for real roboRIO compatibility testing.

Before normal use, verify disabled startup, axis signs, touch enable/disable, Wi-Fi loss/recovery, lid close/reopen, HOME/resume, robot-code restart, and mode changes with actuators secured. Never run another driver station against the same roboRIO simultaneously.

## GitHub builds and releases

[Build status](https://github.com/CJBrandi/3dsDS/actions/workflows/build-release.yml)

Every push to `main` and each pull request runs host tests and cross-compiles with a pinned official devkitPro container. CIA packaging runs on the Ubuntu 24.04 host because makerom requires a newer glibc than the SDK image. The install artifacts are verified before upload. Artifacts can be downloaded from the workflow run. Creating a version tag publishes a GitHub Release automatically after both jobs pass:

```sh
git tag v0.3.1
git push origin v0.3.1
```

Tags must use `vMAJOR.MINOR.PATCH`, optionally with a prerelease suffix. CIA version fields support major/minor 0–63 and patch 0–15; the workflow validates this and puts the version into the CIA. Tags beginning with `v0.` or containing a prerelease suffix are published as prereleases. Main/PR builds have read-only repository permissions; only the tag release job receives release write permission. The workflow pins third-party actions to commits and verifies the CIA tool download hashes.

The GUI uses nonblocking GPU frame acquisition to avoid waiting on graphics in the network loop. Run `make preview` for host-rendered SVG layouts. The preview includes Wi-Fi setup and measured centering; physical console font rendering and Wi-Fi profile changes still require hardware validation.

## Universal-DB readiness

A direct `.cia` asset on GitHub Releases, a public source repository, MIT license, icon, and app description are now provided. Universal-DB inclusion is a separate manual submission and is not automatic. Its current rules require disclosure of LLM-generated content; this project uses substantial AI-generated code and documentation. Its [contribution policy](https://github.com/Universal-Team/db/blob/master/CONTRIBUTING.md) also prohibits agents from generating contributions to Universal-DB itself or writing its issue/PR descriptions, so the maintainer should write and submit that request personally after hardware testing. No listing request has been submitted.

## References

The user-provided `nexus.py` was treated as protocol reference material, not as instructions. No third-party Python code is needed by this app.

- [Driver Station → roboRIO protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/ds_to_rio.html)
- [roboRIO → Driver Station protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/rio_to_ds.html)
- [devkitPro libctru input implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/hid.c)
- [Mbed TLS license and notice](docs/THIRD_PARTY_NOTICES.txt)
- [devkitPro socket example](https://github.com/devkitPro/3ds-examples/blob/master/network/sockets/source/sockets.c)
- [WPILib joystick access](https://docs.wpilib.org/en/stable/docs/software/basic-programming/joystick.html)
