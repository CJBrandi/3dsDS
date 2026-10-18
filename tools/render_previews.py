#!/usr/bin/env python3
"""Render the actual UI layout. Development-only: Pillow + rsvg-convert."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent.parent
subprocess.run(["make", "preview"], cwd=root, check=True)
out = root / "docs/renders"
out.mkdir(exist_ok=True)
font_path="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
def fix_centered_text(path):
    tree=ET.parse(path)
    for text in tree.iter("{http://www.w3.org/2000/svg}text"):
        if "data-center-y" not in text.attrib:
            continue
        size=float(text.attrib["font-size"])
        # Use the actual label's ink bounds relative to its baseline. librsvg
        # does not implement the dominant-baseline attribute used previously.
        font=ImageFont.truetype(font_path,round(size))
        _,top,_,bottom=font.getbbox(text.text or "",anchor="ls")
        cy=float(text.attrib.pop("data-center-y"))
        text.set("y",str(cy-(top+bottom)/2))
    ET.register_namespace("","http://www.w3.org/2000/svg")
    tree.write(path,encoding="unicode")
for path in (root/"docs/top.svg",root/"docs/bottom.svg"):
    fix_centered_text(path)
scenes = [("drive", 0, None), ("setup", 1, None), ("inputs", 2, None),
          ("wifi", 3, None), ("enabled", 0, "enabled"), ("mode-open", 0, "mode"),
          ("alliance-open", 0, "alliance"), ("networks-open", 3, "networks"), ("password-show", 3, "show"), ("wifi-missing",3,"missing")]
for name, page, popup in scenes:
    build = root / "build/previews" / name
    build.mkdir(parents=True, exist_ok=True)
    args = [str(root / "build/preview_ui"), str(build), str(page)]
    if popup:
        args.append(popup)
    subprocess.run(args, check=True)
    fix_centered_text(build / "bottom.svg")
    subprocess.run(["rsvg-convert", str(build / "bottom.svg"), "-o", str(out / (name + ".png"))], check=True)
subprocess.run(["rsvg-convert", str(root / "docs/top.svg"), "-o", str(out / "status.png")], check=True)
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14)
def sheet(filename, items):
    result = Image.new("RGB", (664, 550), "#050a10")
    draw = ImageDraw.Draw(result)
    for i, (name, label) in enumerate(items):
        x, y = 8 + (i % 2) * 332, 8 + (i // 2) * 275
        draw.text((x, y), label, fill="#ecf4fa", font=font)
        result.paste(Image.open(out / (name + ".png")), (x, y + 25))
    result.save(out / filename)
sheet("screens-closed.png", [("drive", "Drive / selectors closed"), ("setup", "Setup"),
                             ("inputs", "Inputs"), ("wifi", "Wi-Fi / selectors closed")])
sheet("selectors-open.png", [("mode-open", "Mode selector open"), ("alliance-open", "Alliance selector open"),
                             ("networks-open", "Found networks selector"), ("password-show", "Wi-Fi / password visible")])
result = Image.new("RGB", (440, 520), "#050a10")
result.paste(Image.open(out / "status.png"), (20, 12))
result.paste(Image.open(out / "drive.png"), (60, 266))
result.save(root / "docs/ui-preview.png")
result = Image.new("RGB", (660, 500), "#050a10")
for name, xy in [("setup", (0, 0)), ("inputs", (340, 0)), ("enabled", (0, 260)), ("wifi", (340, 260))]:
    result.paste(Image.open(out / (name + ".png")), xy)
result.save(root / "docs/ui-states.png")
print(out)
