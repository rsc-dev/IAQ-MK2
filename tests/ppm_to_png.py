# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow>=10"]
# ///
"""Convert every .ppm in a directory to .png (dashboard previews)."""
import sys
from pathlib import Path

from PIL import Image

out = Path(sys.argv[1] if len(sys.argv) > 1 else "out")
for ppm in sorted(out.glob("*.ppm")):
    png = ppm.with_suffix(".png")
    with Image.open(ppm) as img:
        img = img.resize((img.width * 3, img.height * 3), Image.NEAREST)
        img.save(png)
    ppm.unlink()
    print(png)
