#!/usr/bin/env python3
"""Check package structure and publish checksums for the built downloads."""
from pathlib import Path
import argparse
import hashlib
import re
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--version", help="Require the embedded CIA title version MAJOR.MINOR.PATCH")
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent / "dist"
assert (root / "3dsDS.smdh").read_bytes()[:4] == b"SMDH", "Invalid SMDH header"
cia = (root / "3dsDS.cia").read_bytes()
header_size, _, _, certs, ticket, tmd, meta, content = struct.unpack_from("<IHHIIIIQ", cia)
assert header_size == 0x2020 and certs and ticket and tmd and content, "Invalid CIA header"
def align64(n):
    return (n + 63) & ~63
content_offset = align64(align64(align64(align64(header_size) + certs) + ticket) + tmd)
assert cia[content_offset + 0x100:content_offset + 0x104] == b"NCCH", "Missing NCCH content"
# This application has one content entry; title ID in its unencrypted NCCH header.
title_id = struct.unpack_from("<Q", cia, content_offset + 0x118)[0]
assert title_id == 0x000400000F3D5200, f"Unexpected title ID: {title_id:016X}"
if args.version:
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", args.version)
    assert match, "Expected version MAJOR.MINOR.PATCH"
    major, minor, patch = map(int, match.groups())
    assert major <= 63 and minor <= 63 and patch <= 15, "Version exceeds CIA limits"
    tmd_offset = align64(align64(align64(header_size) + certs) + ticket)
    # Pinned makerom emits an RSA-2048/SHA-256 TMD signature (0x140 bytes).
    assert struct.unpack_from(">I", cia, tmd_offset)[0] == 0x10004, "Unexpected TMD signature type"
    tmd_body = tmd_offset + 0x140
    assert struct.unpack_from(">Q", cia, tmd_body + 0x4c)[0] == title_id, "TMD title ID mismatch"
    version = struct.unpack_from(">H", cia, tmd_body + 0x9c)[0]
    expected_version = (major << 10) | (minor << 4) | patch
    assert version == expected_version, f"CIA version {version} != {args.version} ({expected_version})"
assert len(cia) >= content_offset + content, "Truncated CIA"
assert b"ac:i\0\0\0\0" in cia[content_offset + 0x200:content_offset + 0xa00], "Missing Wi-Fi configuration service permission"
assert b"nwm::INF" in cia[content_offset + 0x200:content_offset + 0xa00], "Missing Wi-Fi scan service permission"
notices = root.parent / "docs" / "THIRD_PARTY_NOTICES.txt"
(root / "THIRD_PARTY_NOTICES.txt").write_bytes(notices.read_bytes())
files = [root / name for name in ("3dsDS.cia", "THIRD_PARTY_NOTICES.txt")]
(root / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in files))
print("PASS: SMDH, CIA content/title ID, Wi-Fi permissions, notices, SHA256SUMS" +
      (f", version {args.version}" if args.version else ""))
