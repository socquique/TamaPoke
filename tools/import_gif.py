#!/usr/bin/env python3
"""Importador de sprites animados (GIF) para TamaPoke.

Convierte GIFs (como los 3D de Project Pokemon) a frames 48x48 listos para el
juego: recorte estable entre frames, reduccion de frames a keyframes, escalado,
alpha binario y cuantizacion de colores por especie.

  python3 tools/import_gif.py            # convierte downloads/*.gif y
                                         # renderiza tools/import_sheet.png
"""
import os
import sys
from PIL import Image

CELL = 48          # rejilla del juego
KEYFRAMES = 4      # frames de idle que conservamos
MAX_COLORS = 24    # paleta maxima por especie
ALPHA_T = 96       # umbral de alpha binario


def load_frames(path):
    im = Image.open(path)
    n = getattr(im, 'n_frames', 1)
    frames = []
    for i in range(n):
        im.seek(i)
        frames.append(im.convert('RGBA'))
    return frames


def union_bbox(frames):
    # caja comun a todos los frames para que no "baile" al recortar
    x0, y0, x1, y1 = frames[0].size[0], frames[0].size[1], 0, 0
    for f in frames:
        bbox = f.getchannel('A').point(lambda a: 255 if a >= ALPHA_T else 0).getbbox()
        if not bbox:
            continue
        x0, y0 = min(x0, bbox[0]), min(y0, bbox[1])
        x1, y1 = max(x1, bbox[2]), max(y1, bbox[3])
    return (x0, y0, x1, y1)


def pick_keyframes(frames, k=KEYFRAMES):
    if len(frames) <= k:
        return frames
    step = len(frames) / k
    return [frames[int(i * step)] for i in range(k)]


def to_cell(frame, bbox):
    crop = frame.crop(bbox)
    w, h = crop.size
    scale = min(CELL / w, CELL / h, 1.0)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    crop = crop.resize((nw, nh), Image.LANCZOS)
    cell = Image.new('RGBA', (CELL, CELL), (0, 0, 0, 0))
    cell.paste(crop, ((CELL - nw) // 2, CELL - nh))  # apoyado en el suelo
    # alpha binario
    px = cell.load()
    for y in range(CELL):
        for x in range(CELL):
            r, g, b, a = px[x, y]
            px[x, y] = (r, g, b, 255) if a >= ALPHA_T else (0, 0, 0, 0)
    return cell


def quantize_set(cells, max_colors=MAX_COLORS):
    # paleta comun a todos los frames de la especie (que no parpadee el color)
    strip = Image.new('RGBA', (CELL * len(cells), CELL))
    for i, c in enumerate(cells):
        strip.paste(c, (i * CELL, 0))
    rgb = strip.convert('RGB').quantize(colors=max_colors, method=Image.MEDIANCUT)
    rgb = rgb.convert('RGB')
    out = []
    for i in range(len(cells)):
        q = rgb.crop((i * CELL, 0, (i + 1) * CELL, CELL)).convert('RGBA')
        qpx, apx = q.load(), cells[i].load()
        for y in range(CELL):
            for x in range(CELL):
                if apx[x, y][3] == 0:
                    qpx[x, y] = (0, 0, 0, 0)
        out.append(q)
    return out


def convert(path):
    frames = load_frames(path)
    bbox = union_bbox(frames)
    keys = pick_keyframes(frames)
    cells = [to_cell(f, bbox) for f in keys]
    return quantize_set(cells)


def render_sheet(results, path='tools/import_sheet.png', scale=4):
    pad = 12
    bgs = ['#f2efe1', '#141828']
    rows = len(results)
    cols = KEYFRAMES
    cw = CELL * scale + pad
    img_w = len(bgs) * (cols * cw + pad * 2) + pad
    img_h = rows * (CELL * scale + pad + 16) + pad
    from PIL import ImageDraw
    img = Image.new('RGB', (img_w, img_h), '#888888')
    d = ImageDraw.Draw(img)
    for bi, bg in enumerate(bgs):
        gx = pad + bi * (cols * cw + pad * 2)
        for ri, (name, cells) in enumerate(results.items()):
            oy = pad + ri * (CELL * scale + pad + 16)
            for ci, cell in enumerate(cells):
                ox = gx + ci * cw
                d.rectangle([ox, oy, ox + CELL * scale, oy + CELL * scale], fill=bg)
                big = cell.resize((CELL * scale, CELL * scale), Image.NEAREST)
                img.paste(big, (ox, oy), big)
            d.text((gx, oy + CELL * scale + 2), name, fill='#000')
    img.save(path)
    print(f"guardado {path}")


if __name__ == '__main__':
    srcdir = 'tools/downloads'
    results = {}
    for f in sorted(os.listdir(srcdir)):
        if f.endswith('.gif'):
            results[f[:-4]] = convert(os.path.join(srcdir, f))
    if not results:
        print(f"no hay GIFs en {srcdir}")
        sys.exit(1)
    render_sheet(results)
