#!/usr/bin/env python3
"""Check package structure and publish checksums for the built downloads."""
from pathlib import Path
import hashlib
import struct
from zipfile import ZipFile

root = Path(__file__).resolve().parent.parent / "dist"
assert (root / "3dsDS.3dsx").read_bytes()[:4] == b"3DSX", "Invalid 3DSX header"
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
assert len(cia) >= content_offset + content, "Truncated CIA"
with ZipFile(root / "3dsDS-sd.zip") as archive:
    assert archive.testzip() is None
    assert "3ds/3dsDS/3dsDS.3dsx" in archive.namelist()
    assert archive.read("3ds/3dsDS/3dsDS.3dsx") == (root / "3dsDS.3dsx").read_bytes()
files = [root / name for name in ("3dsDS.3dsx", "3dsDS.smdh", "3dsDS.cia", "3dsDS-sd.zip")]
(root / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in files))
print("PASS: 3DSX/SMDH headers, CIA content/title ID, SD archive, SHA256SUMS")
