# Tasks

## 1. Antes de empezar

- [x] 1.1 Comprobar los archivos del usuario contra MAME: los CRC de `kof97.zip` (set `kof97`, NGM-2320) y de `neogeo.zip` (`sp-s2.sp1`, `sfix.sfix`, `sm1.sm1`, `000-lo.lo`) con los `ROM_START` de `neogeo.cpp`. Verificar que todos coinciden y anotar los CRC para `game.cfg`.
- [x] 1.2 Sacar la línea base de los juegos de Capcom en la rama `neogeo` recién creada: `--hash-log` de SF2 CE y Super Turbo, 3,000 cuadros con `--auto`. Verificar que las dos corridas de cada uno coinciden y guardar los logs fuera del repositorio.

## 2. Los archivos y `game.cfg`

- [x] 2.1 `board = neogeo` y los campos `fix`, `bios_set` y `bios` en `machine/romset.h/.cpp` y `tools/gamecfg.py`; carga de P (palabras invertidas), S, M, V y C (entrelazados por pares) y del BIOS desde `neogeo.zip`. Verificar con una prueba que lee `kof97` y `neogeo` y da los tamaños esperados (P 5 MB, C 40 MB, S 128 KB, M 128 KB, V 12 MB, BIOS 128 KB).
- [x] 2.2 Los errores: sin `neogeo.zip`, y con un archivo de otro CRC. Verificar que cada mensaje nombra el archivo y el CRC esperado (escenarios "Falta el BIOS" y "Un archivo distinto").
- [x] 2.3 `games/kof97/`: `game.cfg` (nombre sin dos puntos, `rom_set`, `rom_name`, `board = neogeo`, archivos con CRC) y un README corto. Verificar que `tools/gamecfg.py` lo lee y que los juegos de Capcom siguen generando lo mismo.

## 3. La placa

- [x] 3.1 `machine/neogeo.h/.cpp`: la clase `NeoGeo` (`M68KBus`), con el mapa del MVS y sus espejos, los vectores BIOS/cartucho, los bancos de P2, la RAM, la memoria de respaldo y el registro de accesos sin mapear (una vez por dirección). Verificar que el 68000 arranca desde los vectores del BIOS y corre sin accesos sin mapear repetidos los primeros 60 cuadros.
- [x] 3.2 El tiempo y las interrupciones (decisión 4): 264 líneas, borrado vertical nivel 1 en la línea 240, el temporizador de posición nivel 2 con sus modos, el arranque en frío nivel 3, el acuse en `0x3C000C`, el perro guardián. Verificar que en 600 cuadros hay 600 interrupciones de nivel 1 atendidas.
- [x] 3.3 Las entradas del MVS (jugadores 1 y 2 con cuatro botones, start/select, monedas, servicio, prueba, DIP) desde `--script` y `--auto`. Verificar con una corrida que mete moneda y start (escenario "Moneda y start").
- [x] 3.4 El lado del sonido (decisión 5): Z80 con `sm1`/`M1`, bancos, latches con NMI y un YM2610 mudo. Verificar que el BIOS pasa su prueba de sonido sin error y sigue al juego (escenario "El BIOS no se queda esperando").
- [x] 3.5 El reloj de calendario (uPD4990) con una fecha fija y la memoria de respaldo en el estado. Verificar que el BIOS no marca error de reloj.
- [x] 3.6 `Serialize` y el hash de estado de la placa (68000, Z80, memorias, registros, video). Verificar con `--state-save`/`--state-load` a mitad de una corrida que el resto da los mismos hashes.

## 4. El video LSPC

- [x] 4.1 `video/neogeo_video.h/.cpp`: VRAM, registros de dirección/dato/avance/modo y las dos paletas. Verificar con una prueba que escribe y lee palabras con varios avances (escenario "El programa escribe en la memoria de video").
- [x] 4.2 La capa fija (S1 o `sfix` según `0x3A000B`/`0x3A001B`) y el color de fondo. Verificar con una captura del arranque del BIOS (escenario "El arranque del BIOS").
- [x] 4.3 Los sprites: tiras de SCB1-SCB4, cuadros de 16 x 16 de los gráficos C, volteos, paletas, encadenado y el límite de 96 por línea. Verificar con capturas de la intro de KOF '97 comparadas con las mismas escenas en MAME.
- [x] 4.4 El encogimiento horizontal y vertical con `000-lo.lo`, y la auto-animación de 4 y 8 cuadros. Verificar con capturas de los acercamientos de la intro y la selección (escenario "Encogimiento").

## 5. El programa de prueba

- [x] 5.1 `port/neogeo_main.cpp` y el objetivo `neogeo-run` en CMake (decisión 2) con `--rom`, `--bios`, `--headless`, `--frames`, `--shot-every`, `--shot-dir`, `--hash-log`, `--script`, `--state-save`, `--state-load`; PNG de 320 x 224. Verificar que compila en MSYS2 UCRT64 y que los juegos de Capcom se compilan igual que antes.

## 6. Comprobaciones

- [x] 6.1 KOF '97 sin ventana, 3,600 cuadros con monedas y start por `--script`, capturas cada 300 cuadros. Verificar que llega al logo del MVS, la intro y la selección de equipo, y mostrar las capturas al usuario.
- [x] 6.2 Repetibilidad: dos corridas de 3,000 cuadros con la misma `--script`. Verificar que los `--hash-log` son idénticos (escenario "Dos corridas iguales").
- [x] 6.3 Los juegos de Capcom: repetir la línea base de 1.2 con la rama terminada. Verificar que los hashes son idénticos (escenario "Los juegos de Capcom sin cambios").
- [x] 6.4 Documentar en `docs/` (o el README de `games/kof97`) lo que hace este paso, los archivos que pide y lo que falta (traducción, sonido, ventana, launcher), y anotar en `THIRD_PARTY_NOTICES.md` cualquier código tomado de MAME.
