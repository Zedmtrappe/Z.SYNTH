#!/usr/bin/env python3
"""Renders the ZYNTH app icon (Resources/Icon/zynth-icon.png, 1024 x 1024).

A dark graphite tile in the macOS icon grid with the panel's amber Z mark,
lit from behind. Drawn at 2x and downsampled for clean edges.
Re-run after editing:  python3 tools/make_icon.py
"""
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 2048                       # working resolution (2x)
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources", "Icon", "zynth-icon.png")


def vertical_gradient(size, top, bottom):
    w, h = size
    g = Image.new("RGB", (1, h))
    for y in range(h):
        t = y / (h - 1)
        g.putpixel((0, y), tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return g.resize((w, h))


def main():
    # macOS Big Sur grid: an 824 pt tile centred in 1024 with room for its shadow.
    inset, radius = int(S * 100 / 1024), int(S * 185 / 1024)
    tile_box = (inset, inset, S - inset, S - inset)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle(tile_box, radius=radius, fill=255)

    # Soft drop shadow under the tile.
    canvas = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((tile_box[0], tile_box[1] + 24, tile_box[2], tile_box[3] + 24), radius=radius,
                                             fill=(0, 0, 0, 150))
    canvas = Image.alpha_composite(canvas, shadow.filter(ImageFilter.GaussianBlur(28)))

    # Tile body: graphite, horizontally brushed.
    body = vertical_gradient((S, S), (40, 42, 47), (12, 13, 15)).convert("RGBA")
    rnd = random.Random(7)
    grain = Image.new("L", (S // 8, S), 128)
    px = grain.load()
    for y in range(S):
        row = 128 + rnd.randint(-14, 14)
        for x in range(S // 8):
            px[x, y] = max(0, min(255, row + rnd.randint(-6, 6)))
    grain = grain.resize((S, S), Image.BILINEAR).filter(ImageFilter.GaussianBlur(0.8))
    light = Image.merge("RGBA", (grain, grain, grain, Image.new("L", (S, S), 38)))
    body = Image.alpha_composite(body, light)
    # Broad sheen from the top left.
    sheen = Image.new("L", (S, S), 0)
    ImageDraw.Draw(sheen).ellipse((-S * 0.2, -S * 0.55, S * 0.9, S * 0.45), fill=30)
    sheen = sheen.filter(ImageFilter.GaussianBlur(S * 0.08))
    body = Image.alpha_composite(body, Image.merge("RGBA", (Image.new("L", (S, S), 255),) * 3 + (sheen,)))

    # The Z mark (same proportions as the panel logo).
    h = S * 0.5
    w = h * 0.82
    ox, oy = (S - w) / 2, (S - h) / 2 - S * 0.015
    pts = [(0, 0), (0.82, 0), (0.82, 0.17), (0.27, 0.83), (0.82, 0.83), (0.82, 1), (0, 1), (0, 0.83), (0.55, 0.17), (0, 0.17)]
    poly = [(ox + x * h, oy + y * h) for x, y in pts]
    zmask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(zmask).polygon(poly, fill=255)

    # Back-light bloom around the Z.
    for blur, alpha in ((90, 150), (34, 170)):
        glow = Image.new("RGBA", (S, S), (240, 150, 50, 0))
        glow.putalpha(zmask.filter(ImageFilter.GaussianBlur(blur)).point(lambda v, a=alpha: v * a // 255))
        body = Image.alpha_composite(body, glow)
    # Cast shadow of the Z (it stands slightly proud of the tile).
    zs = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    zs.putalpha(zmask.filter(ImageFilter.GaussianBlur(10)).point(lambda v: v * 120 // 255))
    body = Image.alpha_composite(body, ImageChops.offset(zs, 6, 14))
    # Amber acrylic face: bright at the top edge of each bar, deeper below.
    face = vertical_gradient((S, S), (255, 222, 150), (196, 112, 28)).convert("RGBA")
    face.putalpha(zmask)
    body = Image.alpha_composite(body, face)
    # Lit upper edge of the Z.
    edge = ImageChops.subtract(zmask, ImageChops.offset(zmask, 0, 6))
    hl = Image.new("RGBA", (S, S), (255, 245, 220, 0))
    hl.putalpha(edge.point(lambda v: v * 200 // 255))
    body = Image.alpha_composite(body, hl)

    # Amber light bar under the mark, as on the panel header.
    bar_y = int(oy + h + S * 0.075)
    bar = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(bar).rounded_rectangle((ox, bar_y, ox + w, bar_y + S * 0.012), radius=8, fill=(255, 190, 90, 255))
    bloom = bar.filter(ImageFilter.GaussianBlur(18))
    body = Image.alpha_composite(Image.alpha_composite(body, bloom), bar)

    # Bevel: lit top edge, dark bottom edge, fine outline.
    bevel = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(bevel)
    d.rounded_rectangle(tile_box, radius=radius, outline=(255, 255, 255, 70), width=6)
    top_only = Image.new("L", (S, S), 0)
    ImageDraw.Draw(top_only).rectangle((0, 0, S, S * 0.45), fill=255)
    bevel.putalpha(ImageChops.multiply(bevel.getchannel("A"), top_only))
    body = Image.alpha_composite(body, bevel)
    dark = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(dark).rounded_rectangle(tile_box, radius=radius, outline=(0, 0, 0, 200), width=4)
    body = Image.alpha_composite(body, dark)

    tile = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tile.paste(body, (0, 0), mask)
    canvas = Image.alpha_composite(canvas, tile)
    canvas.resize((1024, 1024), Image.LANCZOS).save(OUT)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
