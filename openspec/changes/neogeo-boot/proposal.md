# Proposal

## Why

El usuario quiere ports nativos de juegos de **Neo Geo MVS**, empezando por **The King of Fighters '97**, como se hizo
con el CPS-1 clásico (SF2 CE, Hyper Fighting, Ghouls). La comunidad lo pidió en tmcleroy/cps2recomp#139 y el autor
dijo que Neo Geo u online son su siguiente trabajo grande, quizá con otro nombre; esto va en el fork, rama `neogeo`.

El Neo Geo usa los **mismos dos procesadores** que el CPS-2 (68000 y Z80), así que el intérprete, los traductores y el
launcher se reutilizan. Lo nuevo es todo lo de alrededor: el mapa de memoria, el BIOS, los bancos del programa, el video
LSPC y el sonido YM2610. Este **paso 1** comprueba que el modelo de la placa es correcto **antes de traducir nada**:
KOF '97 arranca y se ve en el **intérprete del 68000**, en silencio.

## What Changes

- **Placa "Neo Geo MVS" en el toolkit**: `board = neogeo` en `game.cfg`, con su propia `Machine` (la de CPS no se
  toca más que para elegir una u otra). Reutiliza `machine/m68k.cpp` y el resto del frontend:
  - **mapa de memoria del 68000**: programa (P1 en `0x000000`, P2 por bancos en `0x200000`), RAM de trabajo, BIOS en
    `0xC00000`, registros de E/S (`0x300000`, `0x320000`, `0x380000`, `0x3A0000`, `0x3C0000`), memoria de respaldo;
  - **BIOS del jugador**: `neogeo.zip` (MVS `sp-s2.sp1` por defecto; `sfix`, `sm1`, `000-lo.lo`), con su vector y el
    cambio de vectores BIOS/cartucho (`REG_SWPBIOS`/`REG_SWPROM`);
  - **interrupciones**: VBlank, temporizador y la de arranque, con el reloj de 12 MHz y 264 líneas por cuadro;
  - **entradas**: los dos jugadores, monedas, start/select y las DIP del MVS, en los registros del Neo Geo;
  - **el Z80 con su programa y un YM2610 mudo**: el BIOS prueba el lado de sonido al arrancar, así que el Z80 corre
    `sm1`/`M1` y contesta las órdenes como en la placa, pero no suena nada (a diferencia del paso 1 de CPS-1, donde
    bastó detenerlo).
- **Video LSPC**: la capa fija (40x32 de 8x8, `S1` o `sfix`), los sprites en tiras verticales con encogimiento (tabla
  `000-lo.lo`), las paletas y el color de fondo. Lo justo para el logo del BIOS, la intro y la selección de KOF '97.
- **Juego nuevo `games/kof97`**: set `kof97` (NGM-2320) con los CRC de MAME, cartucho sin protección (`neobase` en
  MAME). Sus archivos y el BIOS los pone el jugador; nada entra al repositorio.
- **Prueba sin ventana**: el BIOS arranca, aparece la pantalla del MVS y KOF '97 llega a su intro y a la selección de
  personajes, con capturas en momentos fijos y el mismo hash en dos corridas iguales.

Fuera de alcance (pasos siguientes):
- **Traducir a C++** el programa de KOF '97 y el BIOS con `tools/m68k.py`.
- **Sonido**: el Z80 con el programa `M1` y el YM2610 (ymfm lo tiene).
- **Launcher, instalación y zip**; funciones propias (training, trucos, widescreen, soundtrack).
- **Juegos protegidos o cifrados** (KOF '98 en adelante, Garou, Metal Slug 3...): CMC, SMA y PCM2 van aparte.
- **AES (consola) y Neo Geo CD.**

## Capabilities

### New Capabilities
- `neogeo-board`: la placa Neo Geo MVS en el toolkit: cómo se declara, qué archivos pide (cartucho y BIOS), el mapa de
  memoria y los bancos, las interrupciones, las entradas y el lado del sonido en silencio mientras no haya YM2610.
- `neogeo-video`: el video LSPC: la capa fija, los sprites en tiras con encogimiento, las paletas y el orden de dibujo.

### Modified Capabilities
<!-- Ninguna: la placa CPS-2 y la CPS-1 clásica no cambian; solo se elige otra Machine cuando board = neogeo. -->

## Impact

- **Código nuevo**: `machine/neogeo.cpp` y `.h` (la placa), `video/neogeo_video.cpp` (LSPC), el lector de sets de Neo
  Geo en `machine/romset.cpp` (P, S, M, V, C y el BIOS), `games/kof97/`.
- **Código tocado**: donde el port crea la `Machine` y lee `board` (`port/main.cpp`, `tools/gamecfg.py`), sin cambiar
  lo que pasa con los juegos de CPS.
- **Referencia**: el driver de MAME (`src/mame/snk/neogeo.cpp`, `neogeo_v.cpp`, `neogeo_spr.cpp`), BSD-3-Clause,
  compatible con GPL-3.0; se cita en `THIRD_PARTY_NOTICES.md` si se toma código y no solo datos.
- **Rama `neogeo`** desde `killer-instinct`, en su propia copia de trabajo (`D:\CPS2\cps2recomp-neogeo`), para no tocar
  el HUD sin terminar de `hud-fightcade` en `D:\CPS2\cps2recomp-src`.
- **Archivos del usuario**: `D:\CPS2\NEO GEO\kof97.zip` y `neogeo.zip`, solo para probar en esta PC.
