# Formato de sprites para importar a TamaPoke

Suelta aquí los sprites y el importador (`tools/sprites.py import`, pendiente de
activar cuando haya assets) los convertirá a `species.h`.

## Formato ideal: PNG-strip horizontal

Un PNG por animación, con los frames en fila y todas las celdas del mismo tamaño:

```
tools/assets/
  004_charmander/
    idle.png      ← OBLIGATORIO: 2-4 frames en fila (p.ej. 4 frames 48×48 → 192×48)
    sleep.png     ← opcional, 2 frames
    eat.png       ← opcional, 2 frames
    sad.png       ← opcional, 1-2 frames
    meta.txt      ← opcional: "evolvesTo=005 evolveLevel=16 type=fuego"
  025_pikachu/
    idle.png
```

## Reglas que evitan dolores

1. **Escala nativa**: 1 píxel de arte = 1 píxel de PNG. Sin reescalar, sin suavizado.
   Tamaño de celda recomendado: **48×48** (también vale 32×32, 56×56, 64×64 — el
   importador encaja con vecino-más-cercano a la rejilla del juego).
2. **Transparencia real**: fondo alpha 0, no blanco ni magenta. Alpha binario
   (0 o 255); las semitransparencias se binarizan y pierden el degradado.
3. **Sin antialiasing**: bordes duros de pixel art. Colores libres, sin límite
   de paleta (cada especie lleva su propia paleta al convertir).
4. **Misma celda en todos los frames** de un strip, criatura centrada y apoyada
   en la misma línea de suelo en cada frame (si "baila" entre frames, vibrará).
5. **Nombre de carpeta `NNN_nombre`** para ordenar el dex.

## También se aceptan GIF animados

El importador puede leer GIF directamente (frames + tiempos). Útil para los
sprites animados que circulan por ahí (suelen ser GIF de 96×96 con 20-40 frames):
se reducen a 3-4 keyframes automáticamente. Para arte propio, mejor PNG-strip
(el GIF limita a 256 colores y su transparencia da problemas).

## Cuántos frames usa el juego

- **idle**: bucle a ~3 fps. Con 2 frames ya vive; 4 es lujo.
- **sleep/eat/sad**: 2 frames. Si faltan, el juego usa idle + emote
  (Zzz, corazón, gota de sudor) — totalmente válido, es lo clásico tamagotchi.

## Presupuesto (no es problema)

151 especies × 4 frames × 48×48 × RGB565 ≈ 2.8 MB de los 16 MB de flash.

## Nota legal

Los sprites oficiales son de Nintendo/Game Freak: para uso personal en tu
dispositivo no hay problema práctico, pero no los publiques con el proyecto.
Para publicar: arte propio o packs con licencia libre.
