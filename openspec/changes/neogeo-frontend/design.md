# Design

## Context

- **El port de Capcom** (`port/port.h`, `port/main.cpp`, `port/frontend_sdl.cpp`) gira alrededor de
  `std::unique_ptr<Machine>`: el ciclo del cuadro, el run-ahead, el sonido de QSound, la AST de Capcom, las máquinas de
  reserva y las fuentes del juego (`runtime/game.h`, cuyas funciones reciben `Machine&`).
- **Las piezas que no dependen de la placa**: el menú (`port/menu.*` sobre `recomp-common/menu.*`), los ajustes
  (`recomp-common/settings.*`), la tabla de controles (`port/bindings.h`, `LoadBindings`), el presentador
  (`port/present.*`: GPU o renderer de SDL, shaders, escalado, forma), el dibujo del HUD (`port/hud.*`), y `fight/`
  (solo nombra los bits de botones de `Machine`).
- **El launcher** trata distinto a los juegos de la colección (`GameInfo` de `games.h`, generado por
  `tools/gamecfg.py --collection`; revisión del set con `check_rom_set`; argumentos `--rom`, `--eeprom`, `--settings`,
  `--features`, `--fullscreen`, `--soundtrack`; tabla de controles compartida) y a los de fuera (`OutsideGame`).
- **El port de Neo Geo** hoy: `port/neogeo_main.cpp` (argumentos, sin ventana) y `port/neogeo_window.cpp` (ventana SDL
  propia, teclas fijas), sobre `NeoGeo`.

## Goals / Non-Goals

**Goals:**
- KOF '97 con lo mismo que un juego de Capcom de la colección: página del launcher, controles, menú, save states,
  shaders, features, y un lugar para sus fuentes.
- Cero cambios en el ciclo de Capcom y en sus hashes; cambios mínimos y aditivos en código del autor.

**Non-Goals:**
- Un `Board` abstracto para `Machine` y `NeoGeo` en el port de Capcom (ver decisión 1).
- La AST, el entrenamiento y el juego en línea (changes propios sobre este).
- Run-ahead, máquinas de reserva y el editor de colores para Neo Geo.

## Decisions

### 1. Un ciclo propio del port de Neo Geo sobre las piezas compartidas, no un `Board` abstracto
`port/neogeo_port.h/.cpp` (una `NeoGeoPort`: opciones, la placa, el menú, los ajustes, los controles, el cuadro, los
estados, el HUD) y `port/neogeo_window.cpp` reescrito con el presentador compartido. El port de Capcom no se toca.
- **Por qué**: `Machine` está en todo el port de Capcom (run-ahead, QSound, AST de Capcom, reservas, `fight/`); meter
  una interfaz común es tocar mucho código del autor, con conflictos en cada actualización suya. Las piezas que no
  dependen de la placa ya están separadas: basta usarlas.
- **Costo**: un segundo ciclo de ventana (~500 líneas) que se parece al de Capcom. Se acepta; si el autor hace su
  propio Neo Geo, se mira entonces cómo juntarlos.

### 2. Las fuentes de un juego de Neo Geo: `runtime/neogeo_game.h`
Paralelo a `runtime/game.h`, con las mismas ideas y nombres (`Feature`, comandos, `beforeFrame`, `frame`, HUD,
`Serialize`), pero sus funciones reciben `NeoGeo&`. `NeoGeo` gana lo que las fuentes necesitan leer y escribir sin
tocar su modelo: leer y escribir la RAM de trabajo (`Peek8/16`, `Poke8/16`) y la entrada antes de cada cuadro (ya está:
`players`). El HUD usa `port/hud.*` como los de Capcom.
- **Por qué**: es la puerta de la AST (fuentes que miran las órdenes de sonido) y del entrenamiento (fuentes que leen
  la RAM y alimentan `fight/`), igual que en los juegos de Capcom.

### 3. KOF '97 como juego de la colección
- `tools/gamecfg.py`: `board`, `fix`, `bios` y `bios_set` en `COLLECTION_KEYS`; `GameInfo` con esos campos.
- `launcher/src/description.cpp`: `check_rom_set` para `board = neogeo` usa `ParseNeoGeoList` / `LoadNeoGeoSet` (el
  launcher ya compila `machine/romset.cpp`), con el mensaje de "falta `neogeo.zip` junto al set"; el resto (Play,
  argumentos, Display, Features, carpeta de ajustes) es el de cualquier juego de la colección. Se quitan `kKof97NeoGeo`
  y la entrada `kof97` de `outside_games()`.
- `launcher/CMakeLists.txt`: sin el puente `kof97`. `tools/collection.sh`: los juegos de Neo Geo vuelven a la lista;
  su programa se copia como `<id>.exe` junto al launcher.
- CMake del juego: el programa vuelve a llamarse `<id>` (`kof97.exe`), ya sin puente con quien chocar.

### 4. Los botones A B C D en la tabla compartida
`games/kof97/game.cfg`: `players = 2` y `buttons = A | C | | B | D` (posiciones de la tabla: LP, MP, HP, LK, MK, HK).
Así, en un stick de seis botones ya configurado para Capcom, la fila de arriba da A y C y la de abajo B y D (el
acomodo de Fightcade y de FBNeo para KOF en seis botones). El port traduce cada acción de la tabla a los bits de
`NeoGeo` (posición 0 a A, 1 a C, 3 a B, 4 a D). Un acomodo propio en Controllers lo cambia.

### 5. Estados y argumentos como los de Capcom
Save states con `NeoGeo::Serialize` (más lo de las fuentes), en la carpeta de guardados que da el launcher
(`--eeprom` señala la carpeta `saves`: ahí van también la memoria de respaldo del MVS, para que no sea un "primer
arranque" cada vez, y los estados). El menú, `--menu`, `--features`, `--fullscreen`, `--scale` como en `port/main.cpp`.

## Risks / Trade-offs

- [Las piezas compartidas esperan algo de `Machine`] -> `port/menu.*` usa `TheGame()` para listar features y comandos:
  se le da la lista de `neogeo_game.h` con una función pequeña del lado de Neo Geo, sin cambiar el menú; si algo pide
  `Machine` directamente, se anota y se resuelve con el mínimo cambio aditivo.
- [La memoria de respaldo guardada cambia la primera corrida] -> Las pruebas sin ventana arrancan sin ella (como hoy),
  así sus hashes siguen comparables.
- [Dos ciclos de ventana que divergen] -> El de Neo Geo sigue al de Capcom en teclas y comportamiento; las pruebas de
  `--menu` cubren lo mismo en los dos.
- [El usuario ya probó KOF como programa de fuera] -> La copia de prueba se rehace con el juego en la colección.

## Migration Plan

- Archivar `neogeo-window-sound` (completo) antes de aplicar este, para que sus specs pasen a `openspec/specs`.
- En la copia de prueba del usuario: quitar `kof97.exe` (puente), `NeoGeo\` y poner `kof97.exe` (el juego) junto al
  launcher, con `kof97.zip` y `neogeo.zip` en `ROM\`.
- Rama `neogeo`, local hasta que el usuario pida subirla.

## Open Questions

- Si el menú de los ports necesita una página propia para el MVS (sus DIP, el modo de juego del BIOS); por ahora no.
