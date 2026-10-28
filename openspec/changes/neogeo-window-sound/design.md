# Design

## Context

- **De `neogeo-boot`**: la placa `NeoGeo` (68000, Z80, LSPC, entradas, estado y hash por cuadro) y `neogeo-run`, sin
  ventana. El Z80 ya corre `sm1`/`M1` con sus bancos y sus latches; el YM2610 es mudo (estado "no ocupado", sin
  temporizadores).
- **ymfm** (aaronsgiles/ymfm, commit 81aec25, el que ya usa el fork para el YM2151): `ymfm_opn.h/.cpp` tiene
  `ymfm::ym2610`, con `ymfm_ssg` y `ymfm_adpcm`. Se conecta por `ymfm_interface`: `ymfm_external_read` (los samples
  ADPCM), `ymfm_set_timer` (cuándo vence un temporizador, en ciclos del chip), `ymfm_update_irq` (la línea de
  interrupción), y `save_restore` para el estado.
- **El frontend de Capcom** (`port/frontend_sdl.cpp`) está hecho alrededor de `Port::RunFrame` y `Machine`: no sirve
  tal cual para la placa Neo Geo.
- **El launcher** ya abre juegos de programas propios (`OutsideProgram`/`OutsideGame` en `launcher/src/description.cpp`,
  puentes en `launcher/CMakeLists.txt`): UltraCombo con sus archivos en `run/`, FBNeo con los suyos en `roms/`.

## Goals / Non-Goals

**Goals:**
- KOF '97 jugable en la PC del usuario: ventana, sonido de la placa, controles, abierto desde el launcher o el escritorio.
- El sonido dentro del estado y del hash: dos corridas iguales siguen dando lo mismo (lo pide `neogeo-online`).
- La prueba sin ventana intacta.

**Non-Goals:**
- Menú, HUD, ajustes guardados, shaders, widescreen, entrenamiento; la traducción a C++; el paquete de ForkAde.
- Generalizar el frontend de Capcom para las dos placas (se verá cuando el Neo Geo necesite su menú).

## Decisions

### 1. El YM2610 de ymfm, sin cambios, en `machine/neogeo_sound.*`
Se agregan `third_party/ymfm_opn.*`, `ymfm_ssg.*` y `ymfm_adpcm.*` del mismo commit 81aec25, sin tocar, con su aviso
(BSD-3) junto al del YM2151. `NeoGeoSound` implementa `ymfm_interface`:
- `ymfm_external_read`: ADPCM-A y ADPCM-B leen los samples V del cartucho (KOF '97 no tiene una región aparte para el
  B; MAME usa la misma).
- `ymfm_set_timer`: guarda cuándo vence cada temporizador en ciclos del Z80 (el chip a 8 MHz, el Z80 a 4); la placa los
  revisa al correr el Z80 y llama a `timer_expired`.
- `ymfm_update_irq`: la línea de interrupción del Z80 (`SetInterruptLine`), de nivel.
- **Por qué**: es el mismo chip que MAME usa y el mismo autor que el YM2151 del fork; un YM2610 propio sería meses.

### 2. El sonido avanza con el Z80 y entra en el estado
El YM2610 genera sus muestras (a su frecuencia, unas 55,555 por segundo con 8 MHz) a la par del Z80, línea por línea,
para que las escrituras caigan donde caen en la placa. Su estado (`save_restore`) y el de sus temporizadores van en
`NeoGeo::Serialize`, así que el hash los cubre. `--mute` sigue generando y solo no entrega el sonido: lo que hace el
juego no cambia. Las salidas del chip (FM y ADPCM en estéreo, SSG mono) se mezclan con las proporciones de MAME.

### 3. La ventana: un frontend propio y chico
`port/neogeo_window.cpp` con SDL3 (ya en el proyecto): ventana redimensionable, la imagen en una textura de 320 x 224
dibujada a 4:3 con bandas negras, F11 pantalla completa, F12 captura, Esc o la X salir. El sonido por
`SDL_OpenAudioDeviceStream` a la frecuencia del chip (SDL convierte a la del dispositivo). El ritmo: como los juegos de
Capcom, corre un cuadro cuando la cola de sonido baja de unos tres cuadros, y espera si no; sin sonido, por reloj.
- **Por qué**: separar el frontend de Capcom de su `Machine` es tocar mucho código del autor por algo que el Neo Geo
  todavía no necesita (menú, HUD). Se hace cuando llegue su menú.

### 4. Controles y la moneda
Teclado fijo (spec `neogeo-window`) y hasta dos controles con `SDL_Gamepad` (el primero es el jugador 1): stick o cruz,
oeste A, sur B, norte C, este D (los ataques débiles a la izquierda, como suele jugarse KOF), Start, Back como moneda.
Una moneda dura 40 cuadros desde que se toca, aunque se suelte antes: el BIOS no cuenta una más corta
(`neogeo-boot`: 10 cuadros no contaron, 40 sí). Es parte de la entrada de cada cuadro, así que sigue siendo repetible.

### 5. Un programa por juego, que encuentra sus archivos
El objetivo de CMake de un juego de Neo Geo se llama como el juego (`kof97`) y su programa `<juego>-neogeo` (`kof97-neogeo.exe`: el launcher ya tiene junto a él un puente `kof97.exe`, que lo encontraría a él), con ventana y sin ventana. Sin `--rom`,
busca `<rom_set>.zip` en `roms/` junto al programa (y en la carpeta actual). `make neogeo-run GAME=kof97` lo construye.

### 6. En el launcher como un programa propio
Un `OutsideProgram` para el port de Neo Geo del fork (carpeta `NeoGeo`, archivos en `roms`) y el `OutsideGame` `kof97`
con `{"kof97.zip"}` y `{"neogeo.zip"}`, y el puente `kof97` (sin argumentos: el programa encuentra sus archivos). El
icono es uno propio y sencillo (texto "KOF 97"), no el logo de SNK.
- **Por qué**: así entra como KI y 3rd Strike, con palomita, "Play" y acceso directo, sin tocar cómo el launcher maneja
  los juegos de Capcom.

## Risks / Trade-offs

- [ymfm con más avisos del compilador] -> Como `ymfm_opm.cpp`: sus archivos con `-w`.
- [El ritmo por la cola de sonido en pantallas de 60 Hz] -> 59.19 contra 60: la cola manda, no el vsync; se mide un
  minuto (spec).
- [Las proporciones de la mezcla] -> Las de MAME para el YM2610; se ajustan de oído con el usuario si hace falta.
- [Sin guardar la memoria de respaldo] -> Cada arranque es un "primer arranque" (el reset del perro guardián del cuadro
  247); guardarla en un archivo va con los ajustes, después.
- [El usuario prueba la ventana] -> Las comprobaciones son sin ventana (hashes, capturas, WAV); la ventana y el
  launcher los abre el usuario y dice qué ve y oye.

## Migration Plan

- `neogeo-run` pasa a llamarse como el juego (`kof97`); el README de `games/kof97` lo dice.
- Rama `neogeo`, local hasta que el usuario pida subirla.

## Open Questions

- Si KOF '97 deja la memoria de respaldo con sus ajustes de fábrica cada vez, o si vale la pena guardarla ya.
