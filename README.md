# GB Web — emulador de Game Boy / Game Boy Color en el navegador

Núcleo: [binjgb](https://github.com/binji/binjgb) (C, MIT) — emula **Game Boy
y Game Boy Color** con sonido, compilado a WebAssembly con Emscripten.
Frontend en HTML/canvas/JS puro, con controles táctiles multi-touch y
estética inspirada en la Game Boy original.

## Estructura

```
gb-emu/
├── src/
│   ├── binjgb/      # núcleo del emulador (de terceros, MIT; incluye su LICENSE)
│   ├── wasm_gbc.c   # wrapper: ROM, frames, pantalla, audio, botones, saves
│   └── post.js      # engancha HEAPU8 al objeto Module (ver comentario dentro)
├── build.sh         # compila src/ -> web/gb.js + web/gb.wasm
└── web/
    ├── index.html          # carcasa, pantalla y controles táctiles
    ├── app.js              # bucle de emulación, input, audio, guardado
    └── gb-audio-worklet.js # reproduce el audio en su propio hilo
```

## Compilar

Con Emscripten activado (`source ~/emsdk/emsdk_env.sh`):

```bash
./build.sh
```

## Probar en local

```bash
cd web
python3 -m http.server 8000
```

Abre `http://localhost:8000`. (No vale abrir el `index.html` con `file://`.)

## ROM precargado

La web carga automáticamente `web/rom.gb` (no hay selector de archivo).
Copia tu `.gb` o `.gbc` en `web/` con ese nombre, o cambia `ROM_URL` al
principio de `app.js`. Funcionan tanto juegos de Game Boy como de Game Boy
Color.

## Guardado de partidas

La RAM con batería del cartucho se guarda sola en `localStorage` (clave por
nombre de archivo y tamaño del ROM), solo cuando el juego escribe en ella, y
siempre al ocultar o cerrar la pestaña. Se restaura al volver a abrir.

## Modo rápido (2X)

El botón 2X junto a START/SELECT duplica la velocidad. El audio se silencia
mientras está activo.

## Audio

`AudioWorklet` en su propio hilo. Requiere contexto seguro (`https://` o
`localhost`); en GitHub Pages va bien. El audio se activa con el primer
toque en cualquier botón (política de autoplay de los navegadores).

## Rendimiento

binjgb es más preciso (y algo más pesado) que un núcleo minimalista. Si en
un móvil modesto va a cámara lenta, la primera palanca es el modo sin audio
(quitar `pushAudio()` del bucle en `app.js`).
