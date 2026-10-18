#!/usr/bin/env python3
"""Standard-library-only UDP stand-in; never connects to a real robot."""
import argparse
import socket
import struct
import time


def joystick(packet):
    offset = 6
    while offset < len(packet):
        size = packet[offset]
        if not size or offset + size + 1 > len(packet):
            raise ValueError("truncated tag")
        tag = packet[offset + 1:offset + size + 1]
        if tag[0] == 0x0c:
            count = tag[1]
            axes = struct.unpack(f">{count}b", tag[2:2 + count])
            pos = 2 + count
            buttons_count = tag[pos]
            button_bytes = (buttons_count + 7) // 8
            buttons = int.from_bytes(tag[pos + 1:pos + 1 + button_bytes], "big")
            pos += 1 + button_bytes
            pov_count = tag[pos]
            pov = struct.unpack(f">{pov_count}h", tag[pos + 1:])
            return axes, buttons, pov
        offset += size + 1
    return (), 0, ()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1", help="Use 0.0.0.0 for a 3DS on your LAN")
    parser.add_argument("--drop-after", type=float, default=0, help="Stop replying after N seconds to test link loss")
    parser.add_argument("--no-code", action="store_true", help="Report robot code unavailable")
    args = parser.parse_args()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, 1110))
    print(f"Mock robot listening on {args.bind}:1110; Ctrl-C to stop", flush=True)
    start = time.monotonic()
    last_log = 0
    estop = False
    try:
        while True:
            data, peer = sock.recvfrom(2048)
            if len(data) < 6 or data[2] != 1:
                continue
            seq, version, control, request, alliance = struct.unpack(">HBBBB", data[:6])
            try:
                axes, buttons, pov = joystick(data)
            except (ValueError, IndexError, struct.error):
                continue
            estop |= bool(control & 0x80)
            enabled = bool(control & 4) and not estop and not args.no_code
            now = time.monotonic()
            dropping = args.drop_after > 0 and now - start >= args.drop_after
            if not dropping:
                status = (control & 3) | (4 if enabled else 0) | (0x80 if estop else 0)
                reply = struct.pack(">HBBBBBB", seq, version, status,
                                    0 if args.no_code else 0x20, 12, 128, 0)
                sock.sendto(reply, (peer[0], 1150))
            if now - last_log >= 0.25:
                print(f"{peer[0]} seq={seq} enabled={enabled} estop={estop} "
                      f"mode={control & 3} axes={axes} buttons={buttons:02x} pov={pov} "
                      f"{'DROPPING REPLIES' if dropping else ''}", flush=True)
                last_log = now
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()


if __name__ == "__main__":
    main()
