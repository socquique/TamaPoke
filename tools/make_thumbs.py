#!/usr/bin/env python3
"""Genera /mons/thumbs.bin: miniaturas 40x40 de los 151 para la galeria.

Se derivan del frame 0 de los .bin ya empaquetados (tools/sdcard/mons/NNN.bin),
asi que no requiere descargas. Formato TPTH (little-endian):

  char[4] "TPTH"
  uint16  count
  uint32  offset[count]    (desde el inicio del archivo, 1-based: offset[0]=dex 1)
  blobs:  u8 w, u8 h, u8 palCount, u16 pal[palCount], u8 data[w*h] (0xFF transp.)

  python3 tools/make_thumbs.py
"""
import os
import struct

DIR = os.path.join(os.path.dirname(__file__), 'sdcard', 'mons')
CELL = 40


def read_tpk_frame0(path):
    with open(path, 'rb') as f:
        if f.read(4) != b'TPK1':
            raise ValueError('magic')
        w, h, frames, _ms = struct.unpack('<4H', f.read(8))
        (palcount,) = struct.unpack('<H', f.read(2))
        pal = list(struct.unpack(f'<{palcount}H', f.read(palcount * 2)))
        data = f.read(w * h)  # solo frame 0
    return w, h, pal, data


def shrink(w, h, pal, data):
    # escala a CELL x CELL con vecino mas cercano, conservando aspecto
    scale = min(CELL / w, CELL / h, 1.0)
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    out = bytearray()
    used = {}
    newpal = []
    for y in range(nh):
        sy = min(h - 1, int(y / scale)) if scale < 1 else y
        for x in range(nw):
            sx = min(w - 1, int(x / scale)) if scale < 1 else x
            idx = data[sy * w + sx]
            if idx == 0xFF:
                out.append(0xFF)
                continue
            c = pal[idx]
            if c not in used:
                used[c] = len(newpal)
                newpal.append(c)
            out.append(used[c])
    return nw, nh, newpal, bytes(out)


def main():
    blobs = []
    for dex in range(1, 152):
        path = os.path.join(DIR, f'{dex:03d}.bin')
        w, h, pal, data = read_tpk_frame0(path)
        nw, nh, npal, ndata = shrink(w, h, pal, data)
        if len(npal) > 255:
            raise ValueError(f'{dex}: paleta {len(npal)}')
        blob = struct.pack('<3B', nw, nh, len(npal))
        blob += struct.pack(f'<{len(npal)}H', *npal)
        blob += ndata
        blobs.append(blob)

    head = 4 + 2 + 4 * 151
    offsets, pos = [], head
    for b in blobs:
        offsets.append(pos)
        pos += len(b)

    out = os.path.join(DIR, 'thumbs.bin')
    with open(out, 'wb') as f:
        f.write(b'TPTH')
        f.write(struct.pack('<H', 151))
        f.write(struct.pack('<151I', *offsets))
        for b in blobs:
            f.write(b)
    print(f"guardado {out}: {pos / 1024:.0f} KB, {len(blobs)} miniaturas")


if __name__ == '__main__':
    main()
