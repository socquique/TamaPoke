# Assets de TamaPoke — de dónde salen los sprites

Los sprites NO se editan a mano ni se guardan aquí: se **descargan de sus
fuentes y se empaquetan** con los scripts de `tools/`. Esta carpeta es solo
documentación del flujo (antes contenía una propuesta de importación por PNG
que quedó obsoleta).

## El flujo real

Dos formatos conviven en la microSD (`/mons/`), ambos derivados de PMD SpriteCollab:

| Formato | Script | Fuente | Qué es |
|---|---|---|---|
| **TPK2** `pNNN.bin` / `psNNN.bin` | `pack_pmd.py` | [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC) | Animaciones multi-acción (idle, walk, sleep, eat, hurt, attack, gestos) — usadas en **todo**: pantalla principal y **Pokédex / galería** |
| **TPTH** `thumbs.bin` | `make_thumbs.py` | (deriva de los TPK2) | Miniaturas 40×40 de la galería |

```bash
python3 tools/pack_pmd.py     # los 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/make_thumbs.py  # -> tools/sdcard/mons/thumbs.bin
python3 tools/send_sd.py      # envia todo a la SD de la placa por USB
```

(`s` = variante shiny. `pack_pmd.py` acepta números de Pokédex sueltos,
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

`pack_pmd.py` cachea los PNG originales de SpriteCollab en `tools/pmd_cache/`
(ignorado por git, regenerable). Los `.bin` finales sí se versionan en
`tools/sdcard/mons/` como respaldo.

> Ver [CREDITS.md](../../CREDITS.md) sobre la procedencia y los términos de los
> sprites. Son de terceros: no redistribuir con fines comerciales.
