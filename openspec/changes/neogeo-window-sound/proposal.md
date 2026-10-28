# Proposal

## Why

Con `neogeo-boot`, KOF '97 arranca en la placa Neo Geo del fork y llega a la selección de equipo, pero solo sin ventana
y en silencio. El intérprete del 68000 ya va 8 a 12 veces más rápido que la placa, así que la traducción a C++ no hace
falta para jugar; lo que falta para que el usuario lo juegue en su PC (y para el juego en línea con GekkoNet, change
`neogeo-online`) es **la ventana, el sonido y el launcher**. La traducción queda para el paso siguiente.

## What Changes

- **Sonido: el YM2610** con ymfm (Aaron Giles, BSD-3-Clause, el mismo commit 81aec25 que el fork ya usa para el YM2151
  del CPS-1): sus archivos OPN, SSG y ADPCM sin cambios. FM, SSG, ADPCM-A (V1-V3) y ADPCM-B; sus temporizadores dan la
  interrupción del Z80, como en la placa. El YM2610 mudo de `neogeo-boot` se va.
- **La ventana** del programa de Neo Geo: imagen de 320 x 224 escalada a 4:3, pantalla completa (F11), salir (Esc),
  captura (F12), el ritmo de la placa (59.19 cuadros por segundo) llevado por la cola de sonido como en los juegos de
  Capcom. Sin menú, HUD ni ajustes en este paso.
- **Controles**: teclado y control para dos jugadores (palanca, A-D, start, moneda), con el acomodo que ya usan los
  juegos del launcher; una moneda breve cuenta un crédito (el BIOS necesita la moneda más tiempo que un toque de tecla).
- **Sigue sin ventana**: `--headless` y todo lo de `neogeo-run` (capturas, hashes, `--script`, estados) siguen igual,
  para las pruebas.
- **KOF '97 en el launcher** de la colección, como Killer Instinct y 3rd Strike: un programa de juego propio
  (`kof97-neogeo.exe` en una carpeta `NeoGeo`, con sus sets en `NeoGeo/roms`) que se abre con su puente `kof97`, con la
  palomita verde cuando están `kof97.zip` y `neogeo.zip`, y el acceso directo al escritorio.

Fuera de alcance:
- La traducción a C++, el menú del juego, el HUD, los modos de entrenamiento, el widescreen y los shaders.
- El juego en línea (`neogeo-online`) y el paquete de ForkAde (un change aparte en ForkAde cuando esto funcione).
- Otros juegos de Neo Geo.

## Capabilities

### New Capabilities
- `neogeo-sound`: el YM2610 de la placa Neo Geo: qué suena (FM, SSG, ADPCM-A y B), sus temporizadores y la interrupción
  del Z80, y que el sonido no cambie el estado del juego ni su repetibilidad.
- `neogeo-window`: el programa de Neo Geo con ventana: imagen, escala, pantalla completa, controles, moneda, el ritmo de
  la placa y lo que sigue funcionando sin ventana.
- `neogeo-launcher`: los juegos de Neo Geo en el launcher de la colección: dónde van sus archivos, cómo se reconocen y
  cómo se abren.

### Modified Capabilities
<!-- Ninguna en openspec/specs. Se apoya en neogeo-board y neogeo-video (change neogeo-boot, sin archivar). -->

## Impact

- **Código nuevo**: `machine/neogeo_sound.*` (el YM2610 sobre ymfm), el frontend con ventana del programa de Neo Geo
  (`port/neogeo_main.cpp` y un `port/neogeo_window.cpp`), `third_party/ymfm_opn.*`, `ymfm_ssg.*`, `ymfm_adpcm.*`.
- **Código tocado**: `machine/neogeo.*` (el YM2610 real en lugar del mudo), `CMakeLists.txt` (SDL3 y ymfm para el
  programa de Neo Geo, que pasa a llamarse como el juego), el launcher del fork (`launcher/src/description.cpp`: el
  programa y el juego; `launcher/CMakeLists.txt`: el puente `kof97`) y `THIRD_PARTY_NOTICES.md`.
- **Pruebas**: sin ventana como hasta ahora (hashes, capturas, un WAV del sonido); la ventana la abre el usuario.
- **Los juegos de Capcom**: sin cambios (sus hashes y su launcher como antes).
