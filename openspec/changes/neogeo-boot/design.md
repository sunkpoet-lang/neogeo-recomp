# Design

## Context

- **Lo que se reutiliza tal cual**: el intérprete del 68000 (`machine/m68k.cpp`) habla con cualquier placa por
  `M68KBus` (Read8/16, Write8/16, Fetch16, InterruptAck); el Z80 (`sound/z80.cpp`) por `Z80Bus`; la serialización
  (`Archive`), el lector de zips y CRC (`machine/romset.cpp`), `port/png_write.h` y el formato de `game.cfg`
  (`tools/gamecfg.py`).
- **Lo que está atado al CPS**: `Machine` (`machine/machine.h`, clase `final`, CPS-2 con el CPS-1 añadido por banderas)
  y `Video` (384 x 224, capas de mosaicos y objetos del CPS-A/B). El port (`port/main.cpp`, el HUD, `fight/`,
  `runtime/spare.cpp`) usa `Machine` directamente en decenas de lugares.
- **El Neo Geo, según MAME** (`src/mame/snk/neogeo.cpp`, `neogeo.h`, `neogeo_spr.cpp`, BSD-3-Clause): reloj maestro de
  24 MHz, 68000 a 12 MHz, Z80 a 4 MHz, YM2610 a 8 MHz, puntos a 6 MHz; 384 x 264 por cuadro, visibles 320 x 224.
  Mapa del 68000 (MVS): programa en `0x000000`, bancos de P2 en `0x200000`, RAM en `0x100000`, E/S en `0x300000`
  (jugador 1, prueba, perro guardián), `0x320000` (órdenes al sonido y su respuesta, monedas), `0x340000` (jugador 2),
  `0x380000` (sistema, salidas), `0x3A0000` (registros de sistema: vectores, capa fija, paleta), `0x3C0000` (LSPC),
  `0x400000` (paleta), `0xC00000` (BIOS), `0xD00000` (memoria de respaldo). Interrupciones: borrado vertical nivel 1,
  temporizador de posición nivel 2, arranque en frío nivel 3.
- **Archivos del usuario** (`D:\CPS2\NEO GEO`): `kof97.zip` (P1 1 MB, P2 4 MB, S1, M1, V1-V3, C1-C6) y `neogeo.zip`
  (`sp-s2.sp1`, `sfix.sfix`, `sm1.sm1`, `000-lo.lo` y otros BIOS).

## Goals / Non-Goals

**Goals:**
- KOF '97 arranca desde el BIOS del MVS y llega a su intro y a la selección de equipo, en el intérprete, sin ventana.
- Un modelo de la placa claro y aparte, que los pasos siguientes (traducción, sonido, ventana, launcher) usen sin
  rehacerlo.
- Cero cambios de comportamiento en los juegos de Capcom.

**Non-Goals:**
- Traducir a C++, el YM2610, la ventana con menús y HUD, el launcher, juegos protegidos, AES y CD.
- Precisión de ciclo: como en CPS, una línea es una cantidad fija de tiempo de CPU.

## Decisions

### 1. Una placa aparte, no otra bandera en `Machine`
`machine/neogeo.h/.cpp`: una clase `NeoGeo` que implementa `M68KBus` y `Z80Bus`, con su memoria, sus registros, su
`Run(frames)`, su `Serialize(Archive&)` y su hash. `Machine` no cambia.
- **Por qué**: el Neo Geo comparte con el CPS solo los procesadores. Meterlo con banderas en `Machine` (como el CPS-1)
  llenaría de `if` cada lectura y escritura, y el CPS-1 clásico ya muestra lo enredado que queda.
- **Alternativa descartada**: una interfaz común `Board` para `Machine` y `NeoGeo` ahora. Obligaría a tocar el port y
  `fight/` antes de saber qué necesita el Neo Geo de ellos; se hace en el paso de la ventana.

### 2. Un programa de prueba propio en este paso
`port/neogeo_main.cpp` construye `neogeo-run` (en CMake, solo cuando el juego declara `board = neogeo`), con las
opciones que ya usa el port: `--rom`, `--bios`, `--headless`, `--frames N`, `--shot-every N`, `--shot-dir`,
`--hash-log`, `--script`, `--state-save F`, `--state-load F`. Sin ventana.
- **Por qué**: el port y su frontend usan `Machine` en todos lados; separarlos es trabajo del paso de la ventana. Una
  prueba sin ventana es justo lo que hace falta para comprobar la placa, y respeta que no se hagan clics en el
  escritorio del usuario.
- La ventana (SDL3, ya en el proyecto) se agrega después, cuando `NeoGeo` entre al port.

### 3. Los archivos: el formato de `game.cfg` con campos nuevos
`board = neogeo` y las listas de archivos con sus CRC, como hoy:
- `program` (P1, P2: en palabras invertidas como `ROM_LOAD16_WORD_SWAP` de MAME), `fix` (S1), `sound_program` (M1),
  `samples` (V1-V3), `graphics` (C1-C6: pares impar/par entrelazados byte a byte, como `ROM_LOAD16_BYTE`);
- `bios_set = neogeo` y `bios = sp-s2.sp1:<crc>`, con `sfix`, `sm1` y `000-lo.lo` del mismo set.
El lector de sets (`romset.cpp`) gana la parte del Neo Geo: busca `neogeo.zip` junto al juego o en la carpeta de ROMs,
y su error nombra el archivo y el CRC que faltan.

### 4. El tiempo, línea por línea
Cada cuadro son 264 líneas; en cada línea el 68000 corre 768 ciclos y el Z80 256. El borrado vertical (nivel 1) se da
al empezar la línea 240 (`NEOGEO_VBSTART`, `0xF0`; las visibles son 16-239), y el temporizador de posición (nivel 2) cuenta en puntos según los
registros `0x3C0006-0x3C000A`, con sus modos de recarga (activar, relativo, recargar en borrado, repetir) como en
MAME. El arranque en frío (nivel 3) se da al encender, no en un reinicio.

### 5. El sonido: el Z80 corre, el YM2610 calla
El Z80 corre `sm1` (y luego `M1`) con sus bancos (`0x8000-0xF7FF` según los puertos `0x08-0x0B`), su RAM, el latch de
órdenes (con NMI) y el latch de respuesta. El YM2610 se reduce a sus registros: acepta escrituras, su estado dice "no
ocupado" y sus temporizadores dan la interrupción del Z80 si el programa los usa. No genera sonido.
- **Por qué**: el BIOS manda órdenes al lado de sonido y espera la respuesta; con el Z80 detenido (lo que bastó en CPS-1)
  el arranque puede marcar error o esperar. Así el paso de sonido solo cambia el YM2610 mudo por ymfm.

### 6. El video LSPC, su propio renderer
`video/neogeo_video.h/.cpp`: 64 K palabras de VRAM (SCB1 en `0x0000`, mapa de la capa fija en `0x7000`, SCB2 en
`0x8000`, SCB3 en `0x8200`, SCB4 en `0x8400`), registros de dirección, dato, avance y modo en `0x3C0000-0x3C000E`, dos
bancos de paleta de 4,096 colores. Dibuja por línea: fondo, los sprites en el orden de la lista (hasta 96 por línea),
con encogimiento por la tabla `000-lo.lo` y el encadenado de SCB3, y la capa fija encima. La auto-animación de 4 y 8
cuadros cuenta con el contador del modo del LSPC.
- **Por qué**: el `Video` de CPS no se parece en nada (mosaicos y objetos, 384 de ancho). Se escribe aparte a partir del
  comportamiento documentado en MAME (BSD-3), sin copiar su código si no hace falta; si se toma código, va con su aviso
  en `THIRD_PARTY_NOTICES.md`.

### 7. La prueba: sin ventana, con capturas y hashes
Los criterios del spec se comprueban con `neogeo-run --headless`: capturas PNG en cuadros fijos (el logo del MVS, la
intro, la selección), comparadas a ojo con las mismas escenas en MAME, y `--hash-log` repetido para la repetibilidad.
Los juegos de Capcom se comprueban con su `--hash-log` contra la rama `killer-instinct`.

### 8. Lista para el juego en línea desde ahora
El juego en línea del Neo Geo es del fork (no hay autor al que esperar) y va con **GekkoNet** (HeatXD/GekkoNet,
BSD-2-Clause: rollback, espectadores, replays y su sesión de estrés), en el change `neogeo-online`. Para que ese paso no
rehaga la placa, `NeoGeo` ya sale con lo que GekkoNet pide: las entradas de cada jugador puestas antes de cada cuadro
(`players[2]`), `RunFrame` sin nada que dependa del reloj ni de la ventana, `Serialize` para guardar y cargar, y
`StateHash` para el chequeo de desincronización. Los ajustes que cambian el juego (las DIP) quedan en el estado.

### 9. Cartuchos de prueba propios, en un paso siguiente
Con **neogeosdk** (eaglesoftware777/neogeosdk, MIT: gcc para el 68000 y driver de YM2610 para el Z80, salida en sets
de MAME) se pueden hacer cartuchos pequeños que prueban una sola cosa (encogimiento, capa fija, auto-animación, sonidos
del YM2610). Como son nuestros, pueden vivir en el repositorio y en las pruebas automáticas, cosa que KOF '97 nunca
podrá. No entra en este paso (su cadena de herramientas pesa 276 MB); se usa cuando las capturas de KOF '97 no basten
para un caso o en el paso del sonido. Sus notas de hardware coinciden con lo que hace este renderer (el sprite de
número mayor se dibuja delante, el color de fondo en `$401FFE`).

## Risks / Trade-offs

- [El BIOS espera algo que no se modeló (memoria de respaldo, reloj de calendario uPD4990, tarjeta de memoria)] ->
  Se registran los accesos sin mapear por dirección (una vez cada una) y se agregan los que pida el arranque; el reloj
  de calendario devuelve una fecha fija para que las corridas sean repetibles.
- [El perro guardián reinicia la placa si el programa no lo atiende] -> Se modela como en MAME, pero con un aviso en el
  registro: un reinicio inesperado señala un error del modelo, no un juego colgado.
- [Encogimiento y encadenado mal hechos se ven "casi bien"] -> Se comparan escenas concretas con MAME (las de los
  acercamientos de la intro y la selección), no solo que "se vea algo".
- [Usar el BIOS universal o el del AES] -> Fuera de este paso: solo `sp-s2.sp1`. Otros BIOS, luego.
- [La rama vive aparte de los juegos de Capcom] -> `neogeo` sale de `killer-instinct` y no toca `hud-fightcade`; se
  rebasa cuando esas ramas avancen.

## Migration Plan

- Nada que migrar: es una placa nueva. Los juegos de Capcom no cambian (comprobado por sus hashes).
- La rama `neogeo` queda local hasta que el usuario pida subirla al fork.

## Open Questions

- Si el BIOS del MVS pide la fecha del uPD4990 para la pantalla de arranque o solo para el modo de operador; se ve al
  arrancar.
- Si KOF '97 usa el temporizador de posición (nivel 2) en la intro; MAME lo dirá con su registro, y el modelo lo tiene
  igual.
