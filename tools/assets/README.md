# Assets de TamaPoke — de dónde salen los sprites

Los sprites NO se editan a mano ni se guardan aquí: se **descargan de sus
fuentes y se empaquetan** con los scripts de `tools/`. Esta carpeta es solo
documentación del flujo (antes contenía una propuesta de importación por PNG
que quedó obsoleta).

## El flujo real

Tres formatos conviven en la microSD (`/mons/`), cada uno con su empaquetador:

| Formato | Script | Fuente | Qué es |
|---|---|---|---|
| **TPK2** `pNNN.bin` / `psNNN.bin` | `pack_pmd.py` | [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) | Animaciones multi-acción (idle, walk, sleep, eat, hurt, attack, gestos) de la **pantalla principal** |
| **TPK1** `NNN.bin` / `sNNN.bin` | `pack_sd.py` | [Pokémon Showdown](https://play.pokemonshowdown.com/sprites/gen5ani/) | Sprites B/N animados de la **Pokédex / galería** (y respaldo) |
| **TPTH** `thumbs.bin` | `make_thumbs.py` | (deriva de los TPK1) | Miniaturas 40×40 de la galería |

```bash
python3 tools/pack_pmd.py     # los 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/pack_sd.py      # los 151 + shiny -> tools/sdcard/mons/[s]NNN.bin
python3 tools/make_thumbs.py  # -> tools/sdcard/mons/thumbs.bin
python3 tools/send_sd.py      # envia todo a la SD de la placa por USB
```

(`s` = variante shiny. Ambos empaquetadores aceptan números de Pokédex sueltos,
p. ej. `python3 tools/pack_pmd.py 7 25`.)

## Formatos binarios

Definidos en las cabeceras de cada empaquetador y parseados en `sdmon.cpp`:

- **TPK1** (`SdMon`): `"TPK1"`, `u16 w,h,frames,frameMs`, `u16 palCount`,
  `u16 pal[]` (RGB565), `u8 data[frames*w*h]` (índice de paleta, `0xFF`
  transparente).
- **TPK2** (`PmdMon`): `"TPK2"`, `u8 nActs`, `u16 palCount`, `u16 pal[]`, y por
  acción `u8 id,w,h,nFrames` + `u16 ms[nFrames]` + `u8 data[w*h*nFrames]`.
- **TPTH** (`SdThumbs`): `"TPTH"`, `u16 count`, `u32 offset[count]`, y por
  miniatura `u8 w,h,palCount` + `u16 pal[]` + `u8 data[w*h]`.

El firmware valida tamaños al cargar, así que un `.bin` truncado se rechaza sin
romper nada.

## Caché de descargas

`pack_pmd.py` y `pack_sd.py` cachean los GIF/PNG originales en
`tools/pmd_cache/` y `tools/downloads/` (ignorados por git, regenerables). Los
`.bin` finales sí se versionan en `tools/sdcard/mons/` como respaldo.

> Ver [CREDITS.md](../../CREDITS.md) sobre la procedencia y los términos de los
> sprites. Son de terceros: no redistribuir con fines comerciales.
