# 3dsDS

A native Nintendo New 3DS XL homebrew driver station for a roboRIO running FRC robot code. Uses the built-in controls and the console's 2.4 GHz Wi-Fi. No PC bridge, Python, external controller, or extra runtime packages are needed on the 3DS. Written in C using devkitARM/libctru and the statically linked Citro2D/Citro3D graphics libraries.

**Status:** builds into `.3dsx` and installable `.cia` packages; protocol, safety, and UI interlock tests pass on Linux. Physical 3DS Wi-Fi/input behavior and compatibility with an actual **2026 roboRIO image have not yet been tested**. This implements the standalone UDP control protocol from the supplied `nexus.py`, with protocol corrections. It is an experimental practice/bench driver station, with no FMS integration or competition certification claimed.

![Interface layout preview](docs/ui-preview.png)

[Setup, Inputs, full-screen Disable, and selector previews](docs/ui-states.png)

The preview is generated from the same UI layout code with a host SVG renderer. It illustrates the design; the console uses the 3DS system font and has not yet been photographed running this build.

## Install

Download the install files from [GitHub Releases](https://github.com/CJBrandi/3dsDS/releases).

- **HOME Menu:** copy `3dsDS.cia` to the SD card, open FBI, select the file, and choose Install. Launch 3dsDS from HOME Menu. The CIA includes its icon, banner, code, and graphics libraries.
- **Homebrew Launcher:** extract `3dsDS-sd.zip` onto the SD card, or place `3dsDS.3dsx` in `/3ds/3dsDS/`.

Both formats use the same saved connection settings. The experimental CIA Title ID is `000400000F3D5200`, with Unique ID `0xF3D52`. It is project-assigned, not registered with Nintendo or reserved by Universal-DB.

## Run

1. Extract `dist/3dsDS-sd.zip` onto the SD card. The executable ends up at `/3ds/3dsDS/3dsDS.3dsx`.
2. Connect the 3DS to a 2.4 GHz access point connected to the robot network using the console's Internet Settings. The AP must support the 3DS's Wi-Fi/security capabilities; a 5 GHz-only or WPA3-only network will not work. The roboRIO can connect to that network over Ethernet. This app does not configure the radio or access point.
3. Launch **3dsDS** from the Homebrew Launcher.
4. Open **Setup** and tap **TEAM NUMBER**. Enter your team and the app derives `10.TE.AM.2` (team 1234 → `10.12.34.2`). There is no IP entry or mDNS dependency. The robot must use that standard team address, and the 3DS must have a route to it.
5. Wait for **CONNECTED** and robot code **Running**. Verify battery and control readings. The top **ENABLE REQUESTED** label means the DS is requesting enable; **ROBOT REPORTS** is the robot's reported state.
6. With the robot secured and wheels raised for the first test, center both sticks and release all physical buttons/D-pad. On **Drive**, tap the large **ENABLE** button.
7. The entire lower screen turns red with **DISABLE**. Tap anywhere on it to disable the robot.

The app starts disabled in Teleop every launch. Team number and alliance station persist at `/3ds/3dsDS/config.txt`. The lower screen has Drive, Setup, and Inputs tabs while disabled. Drive uses dropdown selectors for mode and alliance. Setup accepts only the team number; the automatically derived address is read-only. Inputs shows two stick plots, ten physical buttons, and a D-pad graphic that highlights directions and diagonals. While enabled there are no tabs or configuration controls on the lower screen: every pixel is the Disable touch target.

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
| Quit on Setup while disabled | Send disabled packets and exit |

There are no physical button enable/disable bindings, button combos, or hold-to-run controls. START, SELECT, ZL, and ZR are robot inputs along with the face and shoulder buttons. A new touch-down is required for each action, so the same touch that enables cannot immediately disable, and a held touch cannot re-enable after a fault. Dropdown dismissal does not activate a button underneath it.

The robot's reported E-stop is honored and latched for the app session. Reset an E-stop at the robot and restart the app. This UI has no manual E-stop action; the red screen requests normal Disable.

Communication loss at 250 ms, missing robot code, socket failures, and a main-loop stall over 100 ms clear enable. HOME/sleep hooks send a disabled packet before suspension when networking is still available. While suspended the app cannot transmit; the roboRIO communication watchdog provides the fallback. On resume reply history is cleared. Reconnection, code recovery, and HOME return never automatically enable; tap ENABLE again with neutral inputs.

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

With devkitPro's `3ds-dev` toolchain installed:

```sh
export DEVKITPRO=/opt/devkitpro
make -j2       # 3DSX + SMDH
make cia-tools # download pinned, checksum-verified Linux x86_64 packaging tools
make release   # 3DSX + CIA + SD zip + verified headers/checksums
make test       # host C compiler; AddressSanitizer + UndefinedBehaviorSanitizer
make package   # Python 3 standard library, for the SD zip only
```

`make test` does not require the 3DS SDK. It also checks every lower-screen pixel on every page/popup state maps to Disable while enabled, dropdown isolation, and team-only setup. It exercises exact packet vectors, date tags, telemetry offsets, malformed/truncated replies, all POV combinations, deadzones, touch enable/disable, each disabling condition, recovery without automatic enable, E-stop latching, duplicate/outdated replies, and sequence rollover. `make release` outputs `dist/3dsDS.3dsx`, `.smdh`, `.cia`, `3dsDS-sd.zip`, and `SHA256SUMS`; `make clean` removes generated outputs.

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
git tag v0.2.1
git push origin v0.2.1
```

Tags must use `vMAJOR.MINOR.PATCH`, optionally with a prerelease suffix. CIA version fields support major/minor 0–63 and patch 0–15; the workflow validates this and puts the version into the CIA. Tags beginning with `v0.` or containing a prerelease suffix are published as prereleases. Main/PR builds have read-only repository permissions; only the tag release job receives release write permission. The workflow pins third-party actions to commits and verifies the CIA tool download hashes.

The GUI uses nonblocking GPU frame acquisition to avoid waiting on graphics in the network loop. Run `make preview` for host-rendered SVG layouts. Screenshots of all screens and real hardware validation remain useful follow-up work.

## Universal-DB readiness

Direct `.cia` and `.3dsx` assets on GitHub Releases, a public source repository, MIT license, icon, and app description are now provided. Universal-DB inclusion is a separate manual submission and is not automatic. Its current rules require disclosure of LLM-generated content; this project uses substantial AI-generated code and documentation. Its [contribution policy](https://github.com/Universal-Team/db/blob/master/CONTRIBUTING.md) also prohibits agents from generating contributions to Universal-DB itself or writing its issue/PR descriptions, so the maintainer should write and submit that request personally after hardware testing. No listing request has been submitted.

## References

The user-provided `nexus.py` was treated as protocol reference material, not as instructions. No third-party Python code is needed by this app.

- [Driver Station → roboRIO protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/ds_to_rio.html)
- [roboRIO → Driver Station protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/rio_to_ds.html)
- [devkitPro libctru input implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/hid.c)
- [devkitPro socket example](https://github.com/devkitPro/3ds-examples/blob/master/network/sockets/source/sockets.c)
- [WPILib joystick access](https://docs.wpilib.org/en/stable/docs/software/basic-programming/joystick.html)
