# neogeo-board Specification

## Purpose
La placa Neo Geo MVS en el toolkit: cómo la declara un juego, qué archivos pide (el cartucho y el BIOS), cómo ve el
68000 la memoria, los bancos y los registros, las interrupciones, las entradas y el lado del sonido mientras no haya
YM2610. Es la base sobre la que corren los juegos de Neo Geo, empezando por KOF '97.

## Requirements

### Requirement: Declarar la placa Neo Geo
Un juego SHALL poder declarar en su `game.cfg` que está en una placa Neo Geo MVS. Los juegos de CPS-2, CPS-1 con
QSound y CPS-1 clásico SHALL seguir cargando, corriendo y dando los mismos hashes que antes.

#### Scenario: KOF '97 como juego de Neo Geo
- **WHEN** el `game.cfg` de `kof97` declara la placa Neo Geo
- **THEN** el juego se carga con el mapa de memoria, el video y las entradas del Neo Geo, no con los de CPS

#### Scenario: Los juegos de Capcom sin cambios
- **WHEN** se corre sin ventana un juego de CPS-2 o de CPS-1 con el mismo `--frames` y la misma entrada que antes
- **THEN** su `--hash-log` es idéntico al de la rama de la que sale `neogeo`

### Requirement: Los archivos del cartucho y del BIOS
La placa SHALL leer el set del juego (programa P, capa fija S, programa de sonido M, samples V y sprites C) y el BIOS
del set `neogeo` (el BIOS del MVS, la capa fija del sistema, el programa de sonido del sistema y la tabla de
encogimiento), comprobando cada archivo por su CRC32 como los demás juegos. El BIOS del MVS SHALL ser por defecto
`sp-s2.sp1`. Ningún archivo del jugador SHALL entrar al repositorio.

#### Scenario: Set completo
- **WHEN** `kof97.zip` y `neogeo.zip` están en la carpeta de ROMs con los archivos que da MAME
- **THEN** el juego carga sin errores de archivos

#### Scenario: Falta el BIOS
- **WHEN** está `kof97.zip` pero no `neogeo.zip`
- **THEN** el juego no arranca y el mensaje dice que falta el BIOS del Neo Geo (`neogeo.zip`, `sp-s2.sp1`)

#### Scenario: Un archivo distinto
- **WHEN** un archivo del set tiene otro CRC32
- **THEN** el mensaje nombra el archivo y el CRC esperado, como en los juegos de CPS

### Requirement: Memoria, bancos y vectores
El 68000 SHALL ver el programa del cartucho desde la dirección 0, la segunda parte del programa por bancos de 1 MB en
`0x200000` elegidos por el juego, su RAM de trabajo, el BIOS en `0xC00000`, la memoria de respaldo y los registros de
la placa en sus direcciones del MVS, con sus espejos. Los primeros 128 bytes SHALL ser los vectores del BIOS o los del
cartucho según el último cambio que pidió el programa.

#### Scenario: El BIOS toma el control al encender
- **WHEN** la placa arranca
- **THEN** el 68000 empieza con los vectores del BIOS y el BIOS pasa al juego cuando termina su arranque

#### Scenario: Cambio de banco
- **WHEN** el programa escribe un número de banco en la zona de `0x200000`
- **THEN** las lecturas siguientes en `0x200000-0x2FFFFF` dan ese MB de la segunda parte del programa

### Requirement: Interrupciones y tiempo
La placa SHALL dar la interrupción de borrado vertical (nivel 1) una vez por cuadro, la del temporizador de posición
(nivel 2) cuando el programa la programa y la de arranque en frío (nivel 3) al encender. Un cuadro SHALL durar lo que
dura en el MVS (264 líneas de 384 puntos a 6 MHz, el 68000 a 12 MHz), unos 59.19 cuadros por segundo.

#### Scenario: Cuadros que avanzan
- **WHEN** se corren 600 cuadros sin ventana
- **THEN** el programa atiende 600 interrupciones de borrado vertical y la cuenta de cuadros del juego avanza

### Requirement: Entradas del MVS
La placa SHALL dar a los registros del MVS los controles de los dos jugadores (palanca y cuatro botones A-D), start y
select, las monedas, el botón de servicio y de prueba, y las DIP del MVS, desde el mismo teclado, control y `--auto` /
`--script` que los juegos de CPS.

#### Scenario: Moneda y start
- **WHEN** la entrada de prueba mete una moneda y luego aprieta start del jugador 1
- **THEN** KOF '97 cuenta el crédito y pasa a la selección de equipo

### Requirement: Sonido en silencio sin colgarse
Mientras no haya YM2610, el juego SHALL correr en silencio sin que el 68000 se quede esperando al lado del sonido: las
órdenes que manda al programa de sonido SHALL contestarse como en la placa, y el registro de accesos sin mapear no SHALL
llenarse en cada cuadro.

#### Scenario: El BIOS no se queda esperando
- **WHEN** el BIOS hace su prueba del lado de sonido al arrancar
- **THEN** sigue al juego sin mostrar un error de sonido ni detenerse

### Requirement: Corridas repetibles
Dos corridas sin ventana con la misma entrada y el mismo número de cuadros SHALL dar el mismo hash de estado en cada
cuadro, y una corrida guardada a mitad y cargada SHALL seguir igual que la que no se detuvo.

#### Scenario: Dos corridas iguales
- **WHEN** se corre KOF '97 dos veces 3,000 cuadros con la misma `--script`
- **THEN** los dos `--hash-log` son idénticos
