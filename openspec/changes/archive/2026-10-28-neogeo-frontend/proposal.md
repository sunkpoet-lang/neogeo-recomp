# Proposal

## Why

KOF '97 ya se juega en el fork (`neogeo-window-sound`), pero en una ventana propia y chica: teclas fijas, sin menú,
sin `settings.ini`, sin HUD, y en el launcher entra como un programa de fuera ("sus ajustes son suyos"). Lo que el
usuario pidió para los juegos de ForkAde (controles por juego desde el launcher, la música propia AST como en Super
Turbo, el modo de entrenamiento de KOF, Garou y Fatal Fury) y luego el juego en línea con GekkoNet viven todos en las
piezas compartidas de los ports de Capcom: el menú, los ajustes, la tabla de controles del launcher, el HUD, las
"fuentes" de cada juego y `fight/`. Este change es la base: que el port de Neo Geo use esas piezas.

## What Changes

- **KOF '97 como juego de la colección**, no como programa de fuera: su set en la carpeta de ROMs como los de Capcom
  (`kof97.zip` con `neogeo.zip` al lado), la palomita por CRC, su página con "Play", acceso directo, Display y Features,
  y su `settings.ini` y sus guardados en su carpeta, como cualquier juego de la colección. Se quitan su entrada de
  programa de fuera, su puente `kof97` y la carpeta `NeoGeo` (el programa vuelve a llamarse `kof97.exe`, junto al
  launcher como los demás).
- **Controles desde el launcher**: la página Controllers incluye KOF '97 (2 jugadores, botones A B C D). En la tabla
  que comparten los juegos, A, C, B y D caen donde un stick de seis botones de Capcom tiene LP, MP, LK y MK (arriba A
  C, abajo B D), como se acostumbra en Fightcade; se puede dar a KOF un acomodo propio. Las teclas fijas se van.
- **El menú del juego** (Esc, F1, el clic del stick izquierdo, Start+Select): el mismo de los juegos de Capcom, con
  save states (F5/F9, ranuras), moneda, velocidad, pausa y cuadro a cuadro, sonido, las features del juego y salir.
- **La imagen como en los demás**: el presentador compartido (GPU o renderer de SDL, shaders de la página Display,
  escalado), con el 4:3 del MVS.
- **Las "fuentes" de un juego de Neo Geo**: el mismo mecanismo que `runtime/game.h` (features, comandos, antes y
  después de cada cuadro, líneas y cajas del HUD, su estado en los save states), sobre la placa Neo Geo. Es donde se
  enchufan después la AST (`neogeo-ast`) y el entrenamiento (`neogeo-training`), sin tocar la placa.
- **Sin ventana como antes**: `--headless` y las pruebas (`--hash-log`, capturas, `--script`, estados, WAV) siguen.

Fuera de alcance (cada uno su change, sobre este):
- La música propia AST de los juegos de Neo Geo.
- El modo de entrenamiento de KOF, Garou y Fatal Fury (sobre `fight/`, con las direcciones de RAM que documenta
  fbneo-training-mode, con crédito).
- El juego en línea con GekkoNet (`neogeo-online`).
- Más jugadores: el MVS lee dos; los juegos de Capcom de 3 y 4 jugadores (AvP, Dino, Captain Commando) ya los maneja
  la base del autor y entran a ForkAde cuando se agreguen esos juegos.

## Capabilities

### New Capabilities
- `neogeo-collection`: los juegos de Neo Geo como juegos de la colección: su set y el BIOS en la carpeta de ROMs, cómo
  los reconoce y los abre el launcher, su carpeta de ajustes y guardados, y la tabla de controles.
- `neogeo-port-frontend`: el programa de un juego de Neo Geo con las piezas compartidas de los ports: menú, ajustes,
  controles del launcher, save states, presentador y HUD, y las fuentes del juego (features, comandos, cuadro a cuadro).

### Modified Capabilities
<!-- Ninguna en openspec/specs: neogeo-launcher y neogeo-window están en el change neogeo-window-sound (completo, sin
archivar). Este change reemplaza su entrada de programa de fuera y sus teclas fijas; se archiva aquel primero. -->

## Impact

- **Sin tocar la placa** (`machine/neogeo*`, `video/neogeo_video*`): solo la usa.
- **El port de Neo Geo**: `port/neogeo_main.cpp` y `port/neogeo_window.cpp` pasan a un ciclo propio sobre las piezas
  compartidas (`port/menu.*`, `port/present.*`, `port/hud.*`, `port/bindings.h`, `recomp-common/menu.*`,
  `recomp-common/settings.*`) y un `runtime/neogeo_game.h` paralelo a `runtime/game.h`. El ciclo de Capcom
  (`port/main.cpp`, `port/frontend_sdl.cpp`, `Machine`) no cambia.
- **El launcher del fork**: `launcher/src/description.cpp` (KOF entre los juegos de la colección, su revisión del set
  de Neo Geo; se quita el programa de fuera), `launcher/CMakeLists.txt` (sin el puente `kof97`), `tools/gamecfg.py`
  (`board`, `fix`, `bios`, `bios_set` en la tabla de la colección), `tools/collection.sh` (los juegos de Neo Geo con
  los demás otra vez).
- **Pruebas**: sin ventana (hashes, capturas, WAV, `--menu` del menú como en los juegos de Capcom, la prueba del
  launcher con `--screenshot`); con ventana, el usuario.
- **Los juegos de Capcom**: sin cambios (sus hashes y su launcher).
