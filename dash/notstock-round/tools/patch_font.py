#!/usr/bin/env python3
"""Adds the Czech capitals Orbitron lacks (C D E N R T U with caron or ring)
as composite glyphs: the base letter plus the caron / ring Orbitron already
has for S Z A, centred the same way. Writes a renamed copy (Orbitron is a
Reserved Font Name, see assets/fonts/OFL-Orbitron.txt), for
tools/gen_fonts.sh only; the result is not kept in the repository.

    python tools/patch_font.py in.ttf out.ttf
"""
import sys

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphComponent

# new char: base glyph, accent glyph, reference char that has the same
# accent on a capital (its accent height is reused)
ADD = {
    "Č": ("C", "uni030C"), "Ď": ("D", "uni030C"), "Ě": ("E", "uni030C"),
    "Ň": ("N", "uni030C"), "Ř": ("R", "uni030C"), "Ť": ("T", "uni030C"),
    "Ů": ("U", "uni030A"),
}
REF = {"uni030C": "Š", "uni030A": "Å"}


def centre(glyf, name):
    g = glyf[name]
    g.recalcBounds(glyf)
    return (g.xMin + g.xMax) / 2


def main(src, dst):
    f = TTFont(src)
    glyf, cmap = f["glyf"], f.getBestCmap()
    order = list(f.getGlyphOrder())
    for ch, (base, acc) in ADD.items():
        if ord(ch) in cmap:
            continue
        # the reference's accent offset relative to its base's centre
        ref = glyf[cmap[ord(REF[acc])]]
        rbase = next(c for c in ref.components if c.glyphName != acc)
        racc = next(c for c in ref.components if c.glyphName == acc)
        dx = racc.x - centre(glyf, rbase.glyphName)
        x = round(centre(glyf, base) + dx)
        name = "uni%04X" % ord(ch)
        g = Glyph()
        g.numberOfContours = -1
        g.components = []
        for gn, gx, gy in ((base, 0, 0), (acc, x, racc.y)):
            c = GlyphComponent()
            c.glyphName, c.x, c.y, c.flags = gn, gx, gy, 0
            g.components.append(c)
        g.components[0].flags = 0x0200          # USE_MY_METRICS
        glyf.glyphs[name] = g
        order.append(name)
        f["hmtx"][name] = f["hmtx"][base]
        for t in f["cmap"].tables:
            if t.isUnicode():
                t.cmap[ord(ch)] = name
    f.setGlyphOrder(order)
    glyf.glyphOrder = order
    # a Modified Version may not carry the Reserved Font Name
    for rec in f["name"].names:
        if rec.nameID in (1, 3, 4, 6, 16, 17):
            s = rec.toUnicode().replace("Orbitron", "NotStockOrb")
            rec.string = s
    f.save(dst)


if __name__ == "__main__":
    main(*sys.argv[1:3])
