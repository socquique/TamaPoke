# TamaPoke

Tamagotchi con pixel art a color inspirado en la primera generación de Pokémon,
para la **Waveshare ESP32-S3-Touch-AMOLED-1.75** (AMOLED redonda 466×466,
driver CO5300 por QSPI, táctil CST9217 por I2C).

## Hardware

- Placa: [ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)
- Pines tomados del [repo oficial de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75) (ver `pin_config.h`)

## Librerías

Instalar desde el Library Manager del IDE de Arduino (o copiar las versiones del
repo de Waveshare, carpeta `examples/Arduino-v3.3.5/libraries/`):

| Librería | Autor | Uso |
|---|---|---|
| GFX Library for Arduino (`Arduino_GFX`) | moononournation | Pantalla CO5300 por QSPI + framebuffer |
| SensorLib | Lewis He | Táctil CST9217 |

## Configuración del IDE

- Placa: **ESP32S3 Dev Module**
- Flash Size: **16MB**
- PSRAM: **OPI PSRAM** (imprescindible: el framebuffer de 466×466×16bit ≈ 434 KB vive en PSRAM)
- Partition Scheme: 16MB Flash (3MB APP...)
- USB CDC On Boot: **Enabled**

## Cómo se juega

1. Empiezas con un **huevo** que esconde un inicial elegido al azar (Charmander,
   Bulbasaur o Squirtle). Tócalo 3 veces (se va agrietando) o espera 3 min y eclosiona.
2. Cuatro estadísticas que decaen con el tiempo: **COM** (comida), **FEL** (felicidad),
   **ENE** (energía) y **LIM** (limpieza).
3. Botones táctiles del arco inferior:
   - **COMER**: +30 comida (animación de masticar)
   - **JUGAR**: +25 felicidad, gasta energía y comida
   - **LUZ**: dormir/despertar (durmiendo recupera energía y la pantalla baja el brillo)
   - **BAÑO**: limpia las cacas y restaura la limpieza
4. Tocar al bicho = caricia (+felicidad, corazón).

## Sprites: pipeline de diseño

Los sprites son **pixel art 32×32 a color** con estilo propio inspirado en la gen 1
(paleta compartida de ~26 colores, contorno oscuro, sombreado en el borde inferior).
**No se editan a mano**: se construyen en `tools/sprites.py` con primitivas (elipses
con sombreado automático, contorno automático de silueta) más detalles a píxel
(ojos, boca, manchas, grietas).

```bash
python3 tools/sprites.py        # valida y renderiza tools/sheet.png para revisar
python3 tools/sprites.py emit   # regenera species.h y tools/emitted_sprites.js
```

`species.h` está GENERADO: incluye los sprites, la función `spriteColor()`
(carácter → RGB565), los colores de la UI y la tabla `SPECIES[]`.

## Sistema de especies y evolución

Cada especie es una entrada de la tabla `SPECIES[]`:

```
{ nombre, tipo, sprite, escala, evolucionaA, nivelDeEvolución,
  anclas de ojos/boca, color de cuerpo, color de acento }
```

- **Nivel** = 1 + minutos de juego / `MINUTES_PER_LEVEL` (60 por defecto → 1 nivel/hora).
- **Evolución** al estilo gen 1: los iniciales evolucionan al **Nv. 16** y al **Nv. 36**
  (Charmander → Charmeleon → Charizard, etc.), con animación de silueta parpadeante.
- **Factor de cuidados**: dejar cualquier estadística ≤10 cuenta como *descuido*
  (`careMistakes`), y cada descuido retrasa la evolución 1 nivel. Además, en el
  momento de evolucionar ninguna estadística puede estar por debajo de 40, y
  durmiendo no se evoluciona. Cuidado perfecto = evolución a tiempo.
- Las formas evolucionadas se dibujan más grandes (campo `scale`).

### Expresiones sin duplicar sprites

Cada especie declara **anclas** (posición de ojos y boca) en su entrada de la tabla.
El renderer pinta parpadeo, sueño, masticar y tristeza *encima* del sprite base
usando esas anclas y el color de cuerpo, así que cada especie nueva necesita
**un solo sprite**, no cinco. La UI además usa el color de acento del tipo
(fuego/planta/agua) para el nombre, y de noche cambia a fondo oscuro con estrellas
y brillo de pantalla reducido.

### El dex completo (los 151) vive en la microSD

- `tools/dex_data.py` — fuente única: nombres, tipos (color de acento) y cadena
  de evoluciones con niveles de gen 1 (piedras ≈30, intercambio ≈40; Eevee se
  ramifica al azar entre Vaporeon/Jolteon/Flareon).
- `python3 tools/gen_dex.py` regenera `dex.h` (tabla del firmware).
- `python3 tools/pack_sd.py` descarga los GIF animados (pixel art de B/N) y los
  empaqueta a `tools/sdcard/mons/NNN.bin` (~8.7 MB los 151).
- `python3 tools/send_sd.py` los envía a la SD por USB sin sacar la tarjeta.

Los 9 sprites del taller (`species.h`) quedan como respaldo si no hay SD.
La identidad de la mascota es su número de Pokédex (persistido en NVS, con
migración automática desde guardados antiguos).

## Ciclo de vida y Pokédex de criados

Tres formas de terminar un ciclo (las tres dejan un huevo nuevo):

- **Despedida**: en forma final y con 7 días de juego, se despide con ceremonia.
- **Liberar**: pulsación larga (3 s) sobre el bicho → diálogo SI/NO.
- **Escapada**: con las 4 barras a cero durante 1 hora, se escapa (castigo).

Cada especie criada (eclosión y cada evolución) queda registrada en la
**Pokédex de criados** (bitmap en NVS, contador en la pantalla del huevo).

## Huevos por rareza

Las ~79 formas base se reparten en comunes (47), raras (27) y legendarias (5).
El huevo nuevo tira rareza — 70/27/3 % de base — con tres reglas:

- **Sesgo a lo incompleto**: prefiere especies cuya línea evolutiva tenga
  miembros sin registrar (así los 151 son completables; Eevee además evoluciona
  hacia la rama que falte).
- **La despedida bendice**: si el anterior completó su ciclo (despedida de los
  7 días), el siguiente huevo sube a 45 % raro / 10 % legendario.
- **La escapada castiga**: tras un abandono, el siguiente huevo es común seguro.

Los legendarios solo aparecen con 25+ especies registradas. La rareza se
muestra en la pantalla del huevo ("Huevo raro!", "Huevo legendario!?") pero la
especie es sorpresa hasta que eclosiona. El primer huevo de una partida nueva
siempre es un inicial clásico (Bulbasaur/Charmander/Squirtle/Pikachu/Eevee).

## Estructura

- `TamaPoke.ino` — init de pantalla/táctil/SD, bucle de juego, render, botones en arco y consola serie
- `dex.h` — GENERADO: tabla de los 151 (nombre, evolución, nivel, color de tipo)
- `species.h` — GENERADO: sprites de respaldo en flash, iconos de botones y colores de UI
- `sdmon.h` / `sdmon.cpp` — sprites animados desde la SD (formato TPK1) + recepción de archivos por USB
- `pet.h` / `pet.cpp` — estado de la mascota: huevo, eclosión, descuidos, evolución, persistencia NVS
- `tools/` — taller de sprites, empaquetador del dex, envío a SD y monitor de toques
- `pin_config.h` — pines oficiales de la placa

Para probar rápido: baja `PET_TICK_MS` (velocidad de las estadísticas) y
`MINUTES_PER_LEVEL` (velocidad de niveles/evolución) en `pet.h`.

## Ideas siguientes

- Evoluciones ramificadas (condición por estadística dominante o "piedras" como objetos)
- Progresión offline real con el RTC PCF85063 (qué pasó mientras estaba apagado)
- Minijuego para JUGAR (atrapar la bola al estilo gen 1)
- Sonidos por el ES8311 (bips estilo Game Boy)
- Pokédex: galería de especies ya criadas, guardada en NVS
- Protección del AMOLED: salvapantallas/atenuado tras inactividad para evitar burn-in
