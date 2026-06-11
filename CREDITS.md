# Créditos

TamaPoke es un proyecto **sin ánimo de lucro, para uso personal**. No vende ni
redistribuye comercialmente ningún material con derechos. Pokémon y todos los
nombres, diseños y personajes relacionados son marcas registradas y © de
**Nintendo / Game Freak / The Pokémon Company**.

Este proyecto no está afiliado ni respaldado por ninguna de esas empresas.

## Sprites y datos

| Recurso | Fuente | Uso en el proyecto |
|---|---|---|
| **Animaciones de la mascota** (idle, walk, sleep, eat, hurt, attack…) | [PMD Sprite Collaboration (PMDCollab/SpriteCollab)](https://github.com/PMDCollab/SpriteCollab) | Sprites animados estilo Mystery Dungeon que se ven en la pantalla principal, la ficha y el minijuego |
| **Sprites de la Pokédex** (animados B/N) | [Pokémon Showdown](https://play.pokemonshowdown.com/sprites/gen5ani/) (gen 5 ani) | Las miniaturas y la vista detalle de la galería |
| **Estadísticas base de gen 1** | [PokéAPI](https://pokeapi.co) | FUE/DEF/VEL/HP reales de cada especie |

Los sprites de **SpriteCollab** son obra de su comunidad de artistas bajo sus
propios términos (Creative Commons Attribution-NonCommercial 4.0). El crédito
detallado por especie/autor está en el [tracker.json](https://github.com/PMDCollab/SpriteCollab/blob/master/tracker.json)
del repositorio original. Gracias a toda esa comunidad por un trabajo enorme.

> **Importante para reutilizar este repo:** los archivos de sprite empaquetados
> (`tools/sdcard/mons/*.bin`) derivan de las fuentes de arriba. No los
> redistribuyas con fines comerciales. Si publicas el proyecto, lo limpio es
> distribuir **solo el código y los scripts**, y que cada usuario descargue y
> empaquete los sprites de las fuentes originales con `tools/pack_*.py` (o el
> instalador web).

## Software / hardware

| Componente | Autor / fuente |
|---|---|
| GFX Library for Arduino | [moononournation](https://github.com/moononournation/Arduino_GFX) |
| SensorLib (táctil CST9217, RTC PCF85063) | [Lewis He / lewisxhe](https://github.com/lewisxhe/SensorLib) |
| XPowersLib (PMU AXP2101) | [Lewis He / lewisxhe](https://github.com/lewisxhe/XPowersLib) |
| Placa y pines | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75) |
| Instalador web | [ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Nabu Casa) |

El código de TamaPoke (firmware y herramientas) es obra propia.
