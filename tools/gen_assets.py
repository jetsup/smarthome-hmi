#!/usr/bin/env python3
"""Generates src/assets_icons.c (dashboard symbols, 28x28 A8 masks drawn from the website's icon
paths) and src/assets_bg.c (lock-screen backgrounds, 320x240 RGB565 byte-swapped, the display's format).
Usage: python3 tools/gen_assets.py <path to smarthome-ui/src/components/Icon.vue>   (needs numpy, pillow, svgpathtools)"""
import math, re, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from svgpathtools import Arc

SIZE, SS = 28, 8
NAMES = ["power", "lightbulb", "plug", "sliders", "fan", "thermometer", "droplet", "waves", "cloudrain", "flame",
         "snowflake", "wind", "sun", "moon", "radar", "eye", "user", "gauge", "ruler", "clock", "bell", "speaker",
         "music", "alert", "door", "window", "garage", "home", "car", "bed", "sofa", "tv", "camera", "wifi",
         "battery", "shield", "leaf", "star", "heart", "input", "chip", "sparkle", "layers", "settings", "play", "stop"]

def load_icons(vue):
    src = open(vue).read()
    body = src[src.index("const ICONS = {"):]
    body = body[:body.index("\n}\n")]
    icons = {}
    for m in re.finditer(r"^\s*'?([a-z0-9-]+)'?:\s*\[(.*)\],?\s*$", body, re.M):
        icons[m.group(1)] = re.findall(r"'([^']+)'", m.group(2))
    return icons

TOK = re.compile(r"([MmLlHhVvCcSsQqAaZz])|(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)")

def parse_path(d):
    """-> list of polylines (lists of complex points). Handles compact arc flags."""
    pos = 0
    toks = []
    i = 0
    while i < len(d):
        ch = d[i]
        if ch in " ,\n\t":
            i += 1; continue
        if ch.isalpha():
            toks.append(ch); i += 1; continue
        m = re.compile(r"-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?").match(d, i)
        toks.append(m.group(0)); i = m.end()
    polys, cur = [], []
    x = y = sx = sy = 0.0
    lastc = None
    k = 0
    cmd = None
    def num():
        nonlocal k
        v = float(toks[k]); k += 1; return v
    def flag():
        # arc flags may be glued to the next number: "010-8" -> 0, 1, 0, -8
        nonlocal k
        t = toks[k]
        if len(t) > 1:
            toks[k] = t[1:]; return float(t[0])
        k += 1; return float(t)
    def bez(p0, p1, p2, p3):
        return [(1-t)**3*p0 + 3*(1-t)**2*t*p1 + 3*(1-t)*t*t*p2 + t**3*p3 for t in np.linspace(0, 1, 24)[1:]]
    while k < len(toks):
        if toks[k].isalpha():
            cmd = toks[k]; k += 1
            if cmd in "Zz":
                if cur: cur.append(complex(sx, sy)); polys.append(cur); cur = []
                x, y = sx, sy
                continue
        rel = cmd.islower(); c = cmd.upper()
        if c == "M":
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            if cur: polys.append(cur)
            x, y = nx, ny; sx, sy = x, y; cur = [complex(x, y)]
            cmd = "l" if rel else "L"
        elif c == "L":
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            x, y = nx, ny; cur.append(complex(x, y))
        elif c == "H":
            nx = num(); x = nx + x if rel else nx; cur.append(complex(x, y))
        elif c == "V":
            ny = num(); y = ny + y if rel else ny; cur.append(complex(x, y))
        elif c in "CS":
            if c == "C":
                x1, y1, x2, y2, nx, ny = [num() for _ in range(6)]
                if rel: x1 += x; y1 += y; x2 += x; y2 += y; nx += x; ny += y
            else:
                x2, y2, nx, ny = [num() for _ in range(4)]
                if rel: x2 += x; y2 += y; nx += x; ny += y
                x1, y1 = (2*x - lastc[0], 2*y - lastc[1]) if lastc else (x, y)
            cur += bez(complex(x, y), complex(x1, y1), complex(x2, y2), complex(nx, ny))
            lastc = (x2, y2); x, y = nx, ny
            continue
        elif c == "Q":
            x1, y1, nx, ny = [num() for _ in range(4)]
            if rel: x1 += x; y1 += y; nx += x; ny += y
            p0, p1, p2 = complex(x, y), complex(x1, y1), complex(nx, ny)
            cur += [(1-t)**2*p0 + 2*(1-t)*t*p1 + t*t*p2 for t in np.linspace(0, 1, 16)[1:]]
            x, y = nx, ny
        elif c == "A":
            rx, ry, rot = num(), num(), num()
            la, sw = flag(), flag()
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            if rx == 0 or ry == 0:
                cur.append(complex(nx, ny))
            else:
                arc = Arc(complex(x, y), complex(rx, ry), rot, bool(la), bool(sw), complex(nx, ny))
                cur += [arc.point(t) for t in np.linspace(0, 1, 32)[1:]]
            x, y = nx, ny
        lastc = None if c not in "CS" else lastc
    if cur: polys.append(cur)
    return polys

def raster(paths):
    s = SIZE / 24.0 * SS
    img = Image.new("L", (SIZE * SS, SIZE * SS), 0)
    dr = ImageDraw.Draw(img)
    w = 2.0 * s
    for d in paths:
        try:
            polys_ = parse_path(d)
        except Exception as e:
            raise SystemExit(f"bad path {d!r}: {e}")
        for poly in polys_:
            pts = [(p.real * s, p.imag * s) for p in poly]
            if len(pts) > 1:
                dr.line(pts, fill=255, width=int(round(w)), joint="curve")
            for (px, py) in pts[::1 if len(pts) < 60 else 2] + [pts[-1]]:
                dr.ellipse([px - w/2, py - w/2, px + w/2, py + w/2], fill=255)
    return np.array(img.resize((SIZE, SIZE), Image.LANCZOS), dtype=np.uint8)

def c_array(arr):
    flat = arr.flatten().tolist()
    return ",".join(str(v) for v in flat)

HEAD = '''// Generated by tools/gen_assets.py - do not edit.
#include "lvgl.h"
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
'''

def gen_icons(vue, out):
    icons = load_icons(vue)
    lines = [HEAD]
    have = []
    for n in NAMES:
        if n not in icons:
            print("missing icon", n); continue
        a = raster(icons[n])
        ident = n.replace("-", "_")
        lines.append(f"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t icon_{ident}_map[] = {{{c_array(a)}}};")
        lines.append(f"const lv_image_dsc_t icon_{ident} = {{.header.cf = LV_COLOR_FORMAT_A8, .header.magic = LV_IMAGE_HEADER_MAGIC, "
                     f".header.w = {SIZE}, .header.h = {SIZE}, .header.stride = {SIZE}, .data_size = {SIZE*SIZE}, .data = icon_{ident}_map}};")
        have.append(n)
    lines.append("typedef struct { const char* key; const lv_image_dsc_t* img; } symbol_icon_t;")
    lines.append("const symbol_icon_t symbol_icons[] = {" + ",".join(f'{{"{n}", &icon_{n.replace("-", "_")}}}' for n in have) + "};")
    lines.append(f"const int symbol_icon_count = {len(have)};")
    open(out, "w").write("\n".join(lines) + "\n")
    # contact sheet to eyeball the result
    sheet = Image.new("L", (SIZE * 8 + 18, (len(have) // 8 + 1) * (SIZE + 2)), 0)
    for i, n in enumerate(have):
        sheet.paste(Image.fromarray(raster(icons[n])), ((i % 8) * (SIZE + 2), (i // 8) * (SIZE + 2)))
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save("/tmp/icons_sheet.png")
    print("icons:", len(have))

# ---- backgrounds ------------------------------------------------------------------------
W, H = 320, 240

def blob(cx, cy, r, color, strength=1.0):
    yy, xx = np.mgrid[0:H, 0:W]
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
    m = np.clip(1 - d, 0, 1) ** 2 * strength
    return m[..., None] * np.array(color, dtype=np.float32)

def grad(top, bottom):
    t = np.linspace(0, 1, H)[:, None, None]
    return (1 - t) * np.array(top, np.float32) + t * np.array(bottom, np.float32) + np.zeros((H, W, 3), np.float32)

def bg_aurora():
    im = grad((7, 12, 32), (14, 8, 40))
    im += blob(70, 60, 150, (0, 190, 170), 0.9) + blob(250, 90, 140, (120, 70, 220), 0.85) + blob(160, 230, 160, (30, 110, 230), 0.6)
    return im

def bg_dusk():
    im = grad((28, 16, 70), (240, 110, 70))
    yy, xx = np.mgrid[0:H, 0:W]
    sun = np.clip(1 - np.sqrt((xx - 160) ** 2 + (yy - 150) ** 2) / 70, 0, 1) ** 1.5
    im += sun[..., None] * np.array([255, 190, 90], np.float32) * 0.9
    for k, (base, amp, col) in enumerate([(165, 14, (70, 30, 90)), (190, 10, (40, 18, 70)), (215, 8, (18, 8, 40))]):
        ridge = base + amp * np.sin(xx / (38 - k * 6) + k * 2.1) + amp * 0.5 * np.sin(xx / (13 + k * 4) + k)
        mask = (yy > ridge)[..., None]
        im = np.where(mask, np.array(col, np.float32), im)
    return im

def bg_ocean():
    im = grad((3, 22, 52), (2, 70, 110))
    yy, xx = np.mgrid[0:H, 0:W]
    for k in range(6):
        y0 = 70 + k * 28
        wave = y0 + 9 * np.sin(xx / (30 + k * 5) + k * 1.3)
        band = np.clip(1 - np.abs(yy - wave) / 7, 0, 1)
        im += band[..., None] * np.array([40, 170, 200], np.float32) * (0.10 + k * 0.035)
    im += blob(240, 40, 110, (80, 200, 230), 0.35)
    return im

def bg_midnight():
    im = grad((2, 3, 10), (12, 10, 26))
    im += blob(250, 50, 120, (70, 70, 190), 0.45)
    rng = np.random.default_rng(7)
    for _ in range(90):
        x, y = rng.integers(0, W), rng.integers(0, H)
        b = rng.uniform(0.3, 1.0)
        im[y, x] += np.array([255, 255, 255], np.float32) * b
        if b > 0.8 and 0 < x < W - 1: im[y, x - 1] += 80; im[y, x + 1] += 80
    return im

BGS = [("aurora", bg_aurora), ("dusk", bg_dusk), ("ocean", bg_ocean), ("midnight", bg_midnight)]

def to565_swapped(im):
    im = np.clip(im, 0, 255).astype(np.uint16)
    v = ((im[..., 0] >> 3) << 11) | ((im[..., 1] >> 2) << 5) | (im[..., 2] >> 3)
    be = np.stack([(v >> 8) & 0xFF, v & 0xFF], axis=-1).astype(np.uint8)  # byte-swapped (big-endian) like the display
    return be.flatten()

def gen_bgs(out):
    lines = [HEAD]
    for name, fn in BGS:
        im = fn()
        Image.fromarray(np.clip(im, 0, 255).astype(np.uint8)).save(f"/tmp/bg_{name}.png")
        data = to565_swapped(im)
        lines.append(f"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t bg_{name}_map[] = {{{','.join(str(v) for v in data.tolist())}}};")
        lines.append(f"const lv_image_dsc_t bg_{name} = {{.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED, .header.magic = LV_IMAGE_HEADER_MAGIC, "
                     f".header.w = {W}, .header.h = {H}, .header.stride = {W*2}, .data_size = {W*H*2}, .data = bg_{name}_map}};")
    lines.append("const lv_image_dsc_t* const lock_backgrounds[] = {" + ",".join(f"&bg_{n}" for n, _ in BGS) + "};")
    lines.append("const char* const lock_background_names[] = {" + ",".join(f'"{n.capitalize()}"' for n, _ in BGS) + "};")
    lines.append(f"const int lock_background_count = {len(BGS)};")
    open(out, "w").write("\n".join(lines) + "\n")
    print("backgrounds:", len(BGS))

if __name__ == "__main__":
    gen_icons(sys.argv[1], "src/assets_icons.c")
    gen_bgs("src/assets_bg.c")
