#!/usr/bin/env python3
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

root = Path(__file__).resolve().parent.parent
out = root / "dist" / "3dsDS-sd.zip"
with ZipFile(out, "w", ZIP_DEFLATED) as archive:
    for name in ("3dsDS.3dsx", "3dsDS.smdh"):
        archive.write(root / "dist" / name, "3ds/3dsDS/" + name)
    archive.write(root / "README.md", "3ds/3dsDS/README.md")
print(out)
