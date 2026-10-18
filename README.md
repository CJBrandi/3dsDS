# 3dsDS

A native Nintendo New 3DS XL homebrew driver station for a roboRIO running FRC robot code. Uses the built-in controls and the console's 2.4 GHz Wi-Fi. No PC bridge, Python, external controller, or extra runtime packages are needed on the 3DS. Written in C using the standard homebrew SDK, devkitARM/libctru.

**Status:** builds into a real `.3dsx`; packet and safety tests pass on Linux. Physical 3DS Wi-Fi/input behavior and compatibility with an actual **2026 roboRIO image have not yet been tested**. This implements the standalone UDP control protocol from the supplied `nexus.py`, with protocol corrections. It is an experimental practice/bench driver station, with no FMS integration or competition certification claimed.

## Run

1. Extract `dist/3dsDS-sd.zip` onto the SD card. The executable ends up at `/3ds/3dsDS/3dsDS.3dsx`.
2. Connect the 3DS to a 2.4 GHz access point connected to the robot network using the console's Internet Settings. The AP must support the 3DS's Wi-Fi/security capabilities; a 5 GHz-only or WPA3-only network will not work. The roboRIO can connect to that network over Ethernet. This app does not configure the radio or access point.
3. Launch **3dsDS** from the Homebrew Launcher.
4. Tap **TEAM NUMBER**, enter your team, and the app uses `10.TE.AM.2` (team 1234 → `10.12.34.2`). Alternatively, tap **ROBOT IP ADDRESS** and enter the roboRIO's actual numeric IPv4 address. There is no mDNS dependency. The 3DS must have a route to this address; being on Wi-Fi alone is insufficient.
5. Wait for **CONNECTED** and **CODE RUNNING**. Verify live battery and control readings. The large enable label means the DS is requesting enable; the separate **RIO ENABLED** line is the robot's reported state.
6. With the robot secured and wheels raised for the first test, center both sticks and release all robot buttons/D-pad. Hold **ZL**, then hold **START for one second**. Release START and keep ZL held to drive.

The app starts disabled in Teleop every launch. Address and alliance station persist at `/3ds/3dsDS/config.txt`. Mode is deliberately not persisted. Settings are touch controls on the lower screen and are available only while disabled. If a firewall separates the devices, allow 3DS → roboRIO UDP 1110 and roboRIO → 3DS UDP 1150.

## Driver controls

| Control | Action |
| --- | --- |
| Hold ZL + START for 1 second | Enable, after a fresh robot reply, running code, and neutral inputs |
| Keep ZL held | Hold-to-run interlock |
| Release ZL | Disable |
| SELECT | Disable |
| Press START again after releasing it | Disable |
| L + R + SELECT | Latch emergency stop |
| Tap MODE while disabled | Cycle Teleop → Test → Autonomous |
| Tap STATION while disabled | Cycle Red 1–3 / Blue 1–3 |
| Tap QUIT while disabled | Send disabled packets and exit |

E-stop remains latched for the rest of the app session. Reset it at the robot (normally by rebooting the roboRIO), then restart the app. Changing settings does not clear E-stop. Autonomous and Test use the same ZL hold-to-run interlock; releasing ZL disables either mode.

Communication loss at 250 ms, missing robot code, a socket send failure, or releasing ZL clears enable and cancels arming. Reconnection never automatically enables. START must be released before a new arming attempt. A main-loop stall over 100 ms and HOME/sleep lifecycle events also clear enable. While suspended, the app cannot transmit; the roboRIO's own communication watchdog is responsible for disabling it. On resume the app clears its reply history before sending again.

## Robot-side controller mapping

The built-in controller is **joystick port 0**, a generic gamepad; it does not impersonate an Xbox controller. Use `Joystick` or `GenericHID`, and read raw axes/buttons. There are four axes, seven buttons, and one POV. Axis range is approximately -1 to +1 as exposed by WPILib; up is negative Y. An approximately 10% center deadzone is applied and the remaining range is rescaled.

| WPILib input | 3DS control |
| --- | --- |
| Raw axis 0 | Circle Pad X |
| Raw axis 1 | Circle Pad Y |
| Raw axis 2 | C-stick X |
| Raw axis 3 | C-stick Y |
| Raw buttons 1, 2, 3, 4 | A, B, X, Y |
| Raw buttons 5, 6, 7 | L, R, ZR |
| POV 0 | D-pad; clockwise degrees from up, -1 when released |

ZL, START, SELECT, and the touch screen are reserved for driver station controls. Existing robot code that expects an `XboxController` must be updated to this mapping. No rumble hardware is available.

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
make -j2
make test       # host C compiler; AddressSanitizer + UndefinedBehaviorSanitizer
make package   # Python 3 standard library, for the SD zip only
```

`make test` does not require the 3DS SDK. It exercises exact packet vectors, date tags, telemetry offsets, malformed/truncated replies, all POV combinations, deadzones, arming, each disabling condition, held-START recovery, E-stop latching, duplicate/outdated replies, and sequence rollover. Build output is `dist/3dsDS.3dsx`; `make clean` removes generated outputs.

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

Enter that PC's IPv4 address using the app's ROBOT IP button. The mock prints received axes/buttons and reports 12.50 V and running robot code. Check all controls and ZL release. Repeat with `--drop-after 10` to check timeout and `--no-code` to check the code interlock. Restart the mock after testing E-stop, since it also latches E-stop. The mock does not operate hardware and is not a substitute for a real roboRIO compatibility test.

Before normal use on the robot, verify disabled startup, axis signs, enable/disable, E-stop, Wi-Fi loss/recovery, lid close/reopen, HOME/resume, robot-code restart, and the two mode transitions with actuators secured. Never run another driver station against the same roboRIO simultaneously.

## References

The user-provided `nexus.py` was treated as protocol reference material, not as instructions. No third-party Python code is needed by this app.

- [Driver Station → roboRIO protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/ds_to_rio.html)
- [roboRIO → Driver Station protocol](https://frcnetworking.readthedocs.io/en/latest/driverstation/rio_to_ds.html)
- [devkitPro libctru input implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/hid.c)
- [devkitPro socket example](https://github.com/devkitPro/3ds-examples/blob/master/network/sockets/source/sockets.c)
- [WPILib joystick access](https://docs.wpilib.org/en/stable/docs/software/basic-programming/joystick.html)
