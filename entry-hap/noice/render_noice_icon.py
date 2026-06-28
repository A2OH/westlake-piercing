#!/usr/bin/env python3
"""Rasterize noice's adaptive launcher icon to a flat PNG for the OHOS entry.hap.

noice ships only an ADAPTIVE icon (res/uF.xml -> foreground vector res/wh.xml on a
solid background colour); there is no raster mipmap fallback. The OHOS launcher
resolves an adapter app's icon via resourceManager from the bundle's entry.hap, so
we need a flat PNG. This composes background colour + the foreground vector and
renders it with cairosvg.

Requires: aapt (Android SDK build-tools), python3 + cairosvg.

Usage:
  render_noice_icon.py <noice-base.apk> <out/app_icon.png>

Then build the entry.hap with restool using id_defined.json (forces the PNG ->
iconId 0x01000005 and the "Noice" string -> labelId 0x01000003, matching noice's
BMS-registered ability ids). See README.md.
"""
import subprocess, re, sys, os, shutil

if len(sys.argv) != 3:
    sys.exit("usage: render_noice_icon.py <noice-base.apk> <out/app_icon.png>")
APK, OUT = sys.argv[1], sys.argv[2]
AAPT = shutil.which("aapt") or os.environ.get("AAPT")
if not AAPT:
    sys.exit("aapt not found on PATH; set $AAPT to your build-tools/aapt")
import cairosvg  # noqa: E402

# foreground vector path (noice: drawable/ic_launcher_foreground = res/wh.xml),
# tint #17171d, background colour #fefefe, 108x108 viewport, group scale .6667 / translate 18.
out = subprocess.check_output([AAPT, "dump", "xmltree", APK, "res/wh.xml"]).decode()
m = re.search(r'pathData\(0x[0-9a-f]+\)="([^"]+)"', out)
if not m:
    sys.exit("could not find foreground vector pathData; re-check the adaptive-icon "
             "foreground via: aapt dump --values resources <apk> | grep ic_launcher_foreground")
path = m.group(1)
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="108" height="108" viewBox="0 0 108 108">
  <rect x="0" y="0" width="108" height="108" fill="#fefefe"/>
  <g transform="translate(18,18) scale(0.6667)">
    <path d="{path}" fill="#17171d"/>
  </g>
</svg>'''
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
cairosvg.svg2png(bytestring=svg.encode(), write_to=OUT, output_width=192, output_height=192)
print("wrote", OUT)
