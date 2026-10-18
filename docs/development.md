# Development and protocol notes

[Back to the README](../README.md)

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
- [Mbed TLS license and notice](THIRD_PARTY_NOTICES.txt)
- [devkitPro socket example](https://github.com/devkitPro/3ds-examples/blob/master/network/sockets/source/sockets.c)
- [WPILib joystick access](https://docs.wpilib.org/en/stable/docs/software/basic-programming/joystick.html)
