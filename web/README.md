# Instalador web de TamaPoke

Una página de un clic que flashea el firmware y carga los sprites desde el
navegador (Chrome/Edge), sin Arduino ni drivers. Usa
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) para flashear y
**Web Serial** para enviar los sprites a la SD con el protocolo `PUT` del
firmware (el mismo de `tools/send_sd.py`).

## Contenido

- `index.html` — la página (flasheo + cargador de sprites).
- `manifest.json` — configuración de ESP Web Tools (apunta al firmware).
- `firmware/tamapoke.bin` — firmware combinado, flasheable a `0x0`.

## Regenerar el firmware

Tras cambiar el código del firmware:

```bash
bash tools/build_web.sh   # recompila y regenera web/firmware/tamapoke.bin
```

## Probar en local

Web Serial y ESP Web Tools requieren un **contexto seguro**: `https://` o
`http://localhost`. Para probar:

```bash
cd web && python3 -m http.server 8000
# abre http://localhost:8000 en Chrome/Edge
```

## Desplegar (GitHub Pages)

1. En los ajustes del repo → Pages → servir desde la rama `main`, carpeta
   `/web` (o mueve `web/` a `docs/`). Pages da HTTPS automático.
2. La URL queda en `https://<usuario>.github.io/<repo>/`.

> **Pages en repos privados** requiere GitHub Pro/Team. Si haces el repo
> **público** para usar Pages gratis, **saca antes los sprites del historial**
> (`tools/sdcard/mons/*.bin` son de terceros — ver [CREDITS.md](../CREDITS.md)):
> publica solo código + scripts. El `firmware/tamapoke.bin` sí es código propio
> y se puede servir; los sprites los aporta cada usuario en el paso 2 (o los
> genera con `tools/pack_*.py`).

## Flujo del usuario final

1. **Instalar TamaPoke** → flashea el firmware (elige el puerto USB).
2. **Conectar placa** + seleccionar los `.bin` de los sprites → se copian a la
   microSD por USB (barra de progreso). Cierra antes la pestaña del paso 1: solo
   un programa puede usar el puerto a la vez.
3. Reiniciar (botón PWR) y a criar.

## Limitaciones

- Solo **Chrome/Edge** de escritorio (Web Serial no está en Firefox/Safari).
- El cargador de sprites pide los `.bin` al usuario; no los sirve la página (por
  los términos de los sprites). Una mejora futura sería **empaquetar en el
  navegador** descargando los GIF de las fuentes y convirtiéndolos con canvas
  (portar `pack_sd.py`/`pack_pmd.py` a JS), para que el usuario no necesite los
  archivos de antemano.
