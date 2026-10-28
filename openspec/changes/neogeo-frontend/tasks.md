# Tasks

## 1. Antes

- [x] 1.1 Archivar `neogeo-window-sound` (completo, 14/14) con `/opsx:archive`, para que sus specs pasen a `openspec/specs`. Verificar con `openspec list --specs`.
- [x] 1.2 Revisar qué piden de `Machine` o de `TheGame()` las piezas compartidas (`port/menu.*`, `port/present.*`, `port/hud.*`, `port/bindings.h`, `recomp-common/*`) y anotarlo en el diseño. Verificar que la lista cubre todo lo que el ciclo de Neo Geo va a llamar.

## 2. Las fuentes de un juego de Neo Geo

- [x] 2.1 `runtime/neogeo_game.h`: features, comandos, `beforeFrame`/`frame`, HUD y `Serialize` para `NeoGeo&` (decisión 2), y `Peek`/`Poke` de la RAM de trabajo en `NeoGeo`. Verificar con una feature de prueba en `games/kof97/src` (por ejemplo, tiempo infinito escribiendo la RAM) que se ve en el juego sin ventana.

## 3. El port de Neo Geo

- [x] 3.1 `port/neogeo_port.*`: opciones como `port/main.cpp` (`--rom`, `--eeprom`, `--settings`, `--features`, `--fullscreen`, `--scale`, `--menu`, `--keys`, y las de sin ventana), la placa, los ajustes y el cuadro. Verificar que `--headless` da los mismos hashes que `neogeo-window-sound` con la misma `--script`.
- [x] 3.2 Los controles desde `settings.ini` (`LoadBindings` con la tabla del juego o la compartida) y la traducción de acciones a A B C D (decisión 4). Verificar con un `settings.ini` de prueba que cambia un botón.
- [x] 3.3 El menú (decisión 1, `port/menu.*`): save states por ranura, moneda, velocidad, sonido, features, comandos, salir. Verificar con `--menu` sin ventana que guardar y cargar devuelve el mismo hash.
- [x] 3.4 Las teclas mientras se juega (P, O, - =, F5/F9, F6/F7, F2, F11) y la memoria de respaldo en `saves`. Verificar con `--keys` sin ventana el cuadro a cuadro.
- [x] 3.5 La ventana con el presentador compartido y el HUD (`port/present.*`, `port/hud.*`), 4:3 del MVS, los shaders de Display. Verificar que compila y que la captura sin GPU (`--present-test`) sale a 4:3.

## 4. El launcher

- [x] 4.1 `tools/gamecfg.py` y `GameInfo` con `board`, `fix`, `bios`, `bios_set`; `check_rom_set` de Neo Geo; se quita el programa de fuera y el puente `kof97`; `collection.sh` vuelve a copiar `kof97.exe` (decisión 3). Verificar con `--screenshot` la página de KOF '97 con y sin `neogeo.zip` y que los diez juegos de antes siguen igual.
- [x] 4.2 `games/kof97/game.cfg`: `players = 2`, `buttons = A | C | | B | D`. Verificar en la página Controllers (captura) que KOF '97 sale con sus botones y en la tabla compartida.

## 5. Comprobaciones

- [x] 5.1 Los juegos de Capcom: la línea base (SF2 CE y Super Turbo). Verificar hashes idénticos.
- [x] 5.2 Rehacer la copia de prueba `D:\CPS2\CPS-LAUNCHER-prueba-neogeo` con KOF en la colección y que el usuario pruebe: página, controles desde Controllers con su stick, menú, save states, un shader. Verificar con lo que el usuario dice.
- [x] 5.3 README de `games/kof97` y `docs/` (lo de Neo Geo: el port y sus fuentes). Verificar que coinciden con lo que hace.
