#!/usr/bin/env python3
"""Fetch pinned CIA build tools, checking the archive SHA-256 before extraction."""
import hashlib
import io
from pathlib import Path
import sys
import urllib.request
from zipfile import ZipFile

TOOLS = [
    ("makerom", "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip",
     "287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e", "makerom"),
    ("bannertool", "https://github.com/diasurgical/bannertool/releases/download/1.2.0/bannertool.zip",
     "69768596f836acb3e3aeaa66e47c6ba560dde813c6dfcd33c8afc25fe29b7524", "linux-x86_64/bannertool"),
]
root = Path(sys.argv[1] if len(sys.argv) > 1 else "build/cia-tools")
root.mkdir(parents=True, exist_ok=True)
for name, url, digest, member in TOOLS:
    data = urllib.request.urlopen(url, timeout=60).read()
    if hashlib.sha256(data).hexdigest() != digest:
        raise SystemExit(f"Checksum mismatch: {name}")
    with ZipFile(io.BytesIO(data)) as archive:
        target = root / name
        target.write_bytes(archive.read(member))
        target.chmod(0o755)
    print(f"Verified and installed {name}: {target}")
