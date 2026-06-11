# TamaPoke

Tamagotchi inspirado en la primera generación de Pokémon para la
**Waveshare ESP32-S3-Touch-AMOLED-1.75** (AMOLED redonda 466×466, driver CO5300
por QSPI, táctil CST9217 por I2C). Cría a cualquiera de los 151, evoluciónalo,
entrénalo y complétalos todos (shiny incluidos).

> Repo privado. Los sprites son de Nintendo/Game Freak y de los artistas de
> PMD SpriteCollab y Pokémon Showdown — uso personal. Ver **Créditos**.

## Estado

Funcionando en placa. Implementado: los 151 + shiny animados desde microSD,
ciclo de vida completo (huevo por rareza → evolución → despedida/liberación/
escapada), Pokédex de criados con galería, stats de combate (genes +
entrenamiento), ganchos de retención (racha / vínculo / medallas / nombre),
fondos por bioma y hora real, minijuego, saco de entrenamiento, baño animado,
RTC con progresión offline, batería (AXP2101) y botón PWR, atenuado anti-burn-in.

Pendiente: encuentros salvajes / combate (diseñado, sin implementar), sonido,
instalador web, carcasa 3D. Ver **Roadmap**.

## Hardware

- Placa: [ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)
- Pantalla AMOLED redonda 466×466, driver **CO5300** (QSPI)
- Táctil capacitivo **CST9217** (I2C, dirección 0x5A)
- **AXP2101** (gestión de energía + batería + botón PWR), **PCF85063** (RTC),
  ranura microSD, audio ES8311/ES7210 (sin usar aún)
- Pines tomados del [repo oficial de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75) (ver `pin_config.h`)

## Librerías (Arduino IDE / arduino-cli)

| Librería | Autor | Uso |
|---|---|---|
| GFX Library for Arduino (`Arduino_GFX`) | moononournation | Pantalla CO5300 por QSPI + framebuffer en PSRAM |
| SensorLib | Lewis He | Táctil CST9217 + RTC PCF85063 |
| XPowersLib | Lewis He | PMU AXP2101 (batería, brillo, botón PWR) |

## Configuración del IDE / compilar

- Placa: **ESP32S3 Dev Module** · Flash **16MB** · PSRAM **OPI PSRAM**
  (imprescindible: el framebuffer de 466×466×16bit ≈ 434 KB vive en PSRAM) ·
  Partition Scheme con FAT (p.ej. `16M Flash (3MB APP/9MB FATFS)`) ·
  USB CDC On Boot **Enabled**

```bash
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn "$FQBN" .
```

### Cargar los sprites a la SD

El firmware acepta archivos por USB (protocolo PUT con ACK por bloque), así que
no hace falta sacar la tarjeta. La placa formatea la SD a FAT si no monta.

```bash
python3 tools/pack_sd.py        # B/N (galeria): los 151 + shiny -> tools/sdcard/mons/[s]NNN.bin
python3 tools/pack_pmd.py       # PMD (pantalla principal): los 151 + shiny -> p[s]NNN.bin
python3 tools/make_thumbs.py    # miniaturas de la galeria -> thumbs.bin
python3 tools/send_sd.py        # envia tools/sdcard/mons/* a la SD por USB
```

(~58 MB en total; ~40 MB son los PMD. Están versionados en `tools/sdcard/`.)

## Cómo se juega

Empiezas con un **huevo** (un inicial clásico al azar la primera vez). Tócalo
3 veces o espera y eclosiona. A partir de ahí, cuida a tu compañero:

**Cuatro estadísticas** que decaen: **COM** comida, **FEL** felicidad,
**ENE** energía, **LIM** limpieza. Si alguna toca fondo, cuenta como *descuido*.

**Botones (arco inferior, iconos):**
- 🍎 **Comer** → menú de comida: 3 bayas (cada especie tiene una favorita oculta
  que da bonus) y una chuche (+felicidad pero engorda; el peso da pereza).
- ⚽ **Jugar** → minijuego de la pokeball (entrena VELOCIDAD).
- 🌙 **Luz** → dormir/despertar (recupera energía, baja el brillo).
- 🫧 **Baño** → escena de espuma que limpia las cacas.

**Gestos táctiles:**
- Toque en el bicho = caricia (+felicidad, vínculo).
- Deslizar horizontal = abrir la **Pokédex / galería**.
- Deslizar vertical = abrir la **ficha** (3 páginas: Perfil / Combate / Medallas;
  desliza entre ellas; toca el nombre en Perfil para renombrar; en Combate el
  botón "Entrenar fuerza" abre el saco).
- Pulsación larga (3 s) sobre el bicho = diálogo de **soltar**.

**Botón físico PWR:** corto = pantalla on/off · largo (4 s) = apagado real
(el RTC sigue vivo, así que el tiempo pasa aunque esté apagado).

## Sprites: dos fuentes

- **PMD SpriteCollab** (pantalla principal, ficha, minijuego): sprites con
  comportamiento — `tools/pack_pmd.py` empaqueta acciones (Idle, Walk L/R,
  Sleep, Eat, Hurt, Attack, Pose, Nod, DeepBreath) a formato **TPK2** multi-
  acción (`/mons/pNNN.bin`). El motor en `TamaPoke.ino` hace que el bicho
  pasee, gesticule, duerma acurrucado, masque y se duela. Se ancla por los pies
  (fila más baja con contenido), no por el lienzo.
- **B/N animados de Showdown** (Pokédex / galería): `tools/pack_sd.py` → formato
  **TPK1** (`/mons/NNN.bin`). También sirven de respaldo si falta el PMD.
- **Taller propio** (`tools/sprites.py`): 9 sprites a primitivas como respaldo
  sin SD + los iconos de la UI. Genera `species.h`. Revisa en `tools/sheet.png`,
  emite con `python3 tools/sprites.py emit`.

`sdmon.h/.cpp` carga ambos formatos a PSRAM (`SdMon` para TPK1, `PmdMon` para
TPK2) y las miniaturas (`SdThumbs`).

## Pokédex y datos de especie

`tools/dex_data.py` es la **fuente única**: nombre, slug, tipo (color de acento +
bioma de fondo), cadena evolutiva con niveles de gen 1, rarezas e iniciales.
`tools/dex_stats.py` tiene los base stats reales (de PokeAPI). `gen_dex.py` emite
`dex.h` (tabla `DEX_TBL[152]` con todo). La identidad de la mascota es su número
de Pokédex (persistido en NVS).

- **Evolución** estilo gen 1 (niveles 16/36/…; piedras ≈30, intercambio ≈40;
  Eevee se ramifica a la evolución que te falte). Cada descuido la retrasa 1
  nivel; no evoluciona con stats <40 ni durmiendo.

## Stats de combate y entrenamiento

Cada bicho tiene FUE/DEF/VEL = base real de gen 1 × **genes** (90-110 %, tirados
al eclosionar) + nivel + **entrenamiento**:
- VELOCIDAD ← el minijuego
- DEFENSA ← buen cuidado sostenido (12 h sin descuidos)
- FUERZA ← el saco de entrenamiento (aporrear)

Se ven en la página Combate de la ficha. El peso (oculto) sube con chuches y se
quema entrenando.

## Retención: racha, vínculo, medallas, nombre

- **Racha** (del jugador, persiste entre crianzas): el primer cuidado de cada
  día real avanza la racha; hitos 3/7/30/100 se celebran; saltarse un día la
  rompe. Insignia de llama en la pantalla principal.
- **Vínculo** (del bicho): sube despacio con cuidado y mimos, baja con descuidos.
- **Medallas** del individuo (nivel, baya, racha, vínculo, forma final, en
  forma) + contador global. Página Medallas de la ficha.
- **Nombre**: teclado táctil; el apodo manda en la cabecera y la ficha.

La racha y el vínculo altos **mejoran el sorteo del huevo** (rareza y shiny):
cuidar bien siempre paga.

## Ciclo de vida y huevos por rareza

Tres finales (los tres dejan un huevo nuevo): **despedida** (forma final + 7
días), **liberar** (pulsación larga), **escapada** (4 barras a cero 1 h). Cada
especie criada se registra en la **Pokédex de criados** (normal y shiny aparte).

El huevo tira rareza sobre las ~79 formas base (47 comunes / 27 raras / 5
legendarias), con **sesgo a las líneas que te faltan** (los 151 son
completables), bendecido por una despedida y castigado por una escapada.
Legendarios solo con 25+ registradas. **Shiny** 1/48 (mejor con racha/vínculo/
despedida). El primer huevo de la partida es un inicial clásico.

## Fondos: bioma + hora real

La pantalla idle pinta el cielo según la **hora real del RTC** (amanecer / día /
atardecer / noche con luna y estrellas) y el suelo según el **bioma del tipo**
(pradera, playa, bosque, volcán, montaña, nieve). Dormir fuerza la noche.

## Estructura

- `TamaPoke.ino` — init, bucle de juego, render de todas las pantallas, gestos, consola serie
- `pet.h` / `pet.cpp` — estado y lógica de la mascota (stats, evolución, ciclo de vida, racha/vínculo/medallas, NVS)
- `sdmon.h` / `sdmon.cpp` — sprites TPK1 (B/N) y TPK2 (PMD) + miniaturas, y recepción de archivos por USB (PUT/LS)
- `rtcbat.h` / `rtcbat.cpp` — RTC PCF85063 + PMU AXP2101 (batería, brillo, botón PWR)
- `dex.h` — GENERADO (`gen_dex.py`): tabla de los 151
- `species.h` — GENERADO (`sprites.py`): sprites de respaldo, iconos de UI, colores
- `pin_config.h` — pines oficiales de la placa
- `tools/` — pipeline: `dex_data.py` (datos), `dex_stats.py`, `gen_dex.py`,
  `sprites.py` (taller), `pack_sd.py` / `pack_pmd.py` / `make_thumbs.py`
  (empaquetadores), `send_sd.py` (envío a SD), `import_gif.py`, `touch_log.py`
- `tools/sdcard/mons/` — los .bin generados (B/N, shiny, PMD, miniaturas)

## Consola serie (115200, depuración)

`STATS` (estado completo) · `SPEC <dex>` (cambiar especie) · `LVL <n>` ·
`HATCH` · `SHINY` · `NICK <x>` · `BYE` (despedir) · `REG` (Pokédex) ·
`EGGS` (simular 20 huevos) · `GAL` (galería) · `CAREDAY` (simular día de
cuidado) · `TIME <epoch>` / `RTCSET <epoch>` (reloj) · `LS` / `PUT` (archivos SD).

Para probar rápido: baja `PET_TICK_MS` y `MINUTES_PER_LEVEL` en `pet.h`.

## Roadmap

- **Encuentros salvajes / combate** — diseñado (ver memoria del proyecto):
  resolución por FUE/DEF/VEL con animaciones Attack/Hurt PMD, rango de
  entrenador como endgame. Falta elegir estilo (auto / timing / por turnos).
- **Sonido** por el ES8311 (bips estilo Game Boy).
- **Doble framebuffer** para subir los fps del minijuego sin parpadeo.
- **Soak test** 24-48 h.
- **Publicación**: instalador web (ESP Web Tools + empaquetado en JS), `CREDITS.md`,
  carcasa 3D para MakerWorld.
- Conectar la **batería** cuando llegue.

## Créditos

Animaciones de la pantalla principal: [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
(comunidad). Sprites de la Pokédex: [Pokémon Showdown](https://play.pokemonshowdown.com).
Base stats: [PokeAPI](https://pokeapi.co). Pokémon es ™ de Nintendo / Game Freak /
The Pokémon Company. Proyecto sin ánimo de lucro, para uso personal.
