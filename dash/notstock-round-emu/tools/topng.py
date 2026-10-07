"""PPM from tools/sim -> PNG, masked to the round panel."""
import sys
from PIL import Image, ImageDraw
im = Image.open(sys.argv[1]).convert("RGB")
w, h = im.size
bg = Image.new("RGB", (w, h), (24, 24, 24))
m = Image.new("L", (w, h), 0)
ImageDraw.Draw(m).ellipse((0, 0, w - 1, h - 1), fill=255)
bg.paste(im, (0, 0), m)
bg.save(sys.argv[2])
