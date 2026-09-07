# Tests

Bateria de tests que corre **en el PC, sin placa**: compila la logica pura del
firmware (`pet.cpp`, `i18n.cpp`, `dex.h`) contra unos shims de Arduino y la
ejecuta con un reloj y un `random()` falsos, asi que los resultados son
deterministas y tardan menos de un segundo.

```bash
./test/run_tests.sh           # todo
./test/run_tests.sh --asan    # ademas con AddressSanitizer + UBSan
```

O por partes:

```bash
cd test && make               # solo la logica del firmware
cd test && make FILTER=evolve # solo los tests con "evolve" en el nombre
cd test && make asan          # con sanitizers
python3 test/test_tools.py    # solo las herramientas de tools/
```

## Que se cubre

| Fichero | Que comprueba |
|---|---|
| `test_pet.cpp` | huevo y eleccion de inicial, comida, tick de estadisticas, sueno, descuidos, niveles, evolucion (incluida la rama de Eevee), stats de combate, entrenamiento y minijuego, racha y vinculo, medallas, ceremonias, progresion offline y guardado en NVS |
| `test_dex.cpp` | integridad de la Pokedex: nombres ASCII, cadenas evolutivas sin bucles, toda especie alcanzable, ninguna linea imposible de completar, limites de los buffers de la UI |
| `test_i18n.cpp` | los 6 idiomas completos, sin acentos (la fuente GFX es ASCII), y con los mismos `%u`/`%s` que le pasa el sketch en cada llamada |
| `test_tools.py` | los scripts de `tools/` compilan, los datos de la Pokedex son coherentes y `dex.h` sigue coincidiendo con lo que genera `tools/gen_dex.py` |

## Como esta montado

- `shim/Arduino.h` + `shim/Preferences.h` + `shim/shim.cpp`: lo minimo de Arduino
  que usan `pet.cpp` e `i18n.cpp`. El reloj (`millis()`) y `random()` son falsos
  y controlables; `Preferences` es un mapa en memoria que imita la NVS, asi que
  se puede simular un reinicio creando otro `Pet` y llamando a `begin()`.
- `framework.h` + `main.cpp`: micro framework propio, sin dependencias.
- Los tests marcados con `TEST_KNOWN_ISSUE` describen el comportamiento
  **correcto** de algo que hoy falla: no rompen la bateria, salen listados
  aparte, y avisan si algun dia empiezan a pasar (para poder quitar la marca).

## Que NO se cubre

Todo lo que necesita la placa: pantalla y tactil (`TamaPoke.ino`), audio
(`audio.cpp`), tarjeta SD (`sdmon.cpp`) y RTC/bateria (`rtcbat.cpp`). El
`.ino` tampoco se compila aqui: hace falta `arduino-cli` con el core de ESP32.
