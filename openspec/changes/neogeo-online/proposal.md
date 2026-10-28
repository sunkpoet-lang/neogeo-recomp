# Proposal

## Why

Los juegos de Neo Geo del fork (empezando por KOF '97, change `neogeo-boot`) son lo que más se juega en línea en la
comunidad, y aquí el juego en línea es nuestro: no hay un autor al que esperar, como con los juegos de Capcom (que
siguen el netplay de Tommy, tmcleroy/cps2recomp#70). Ya existe todo lo de alrededor: el túnel `cps-tunel` (Iroh, código
largo, directo o por el relay de n0, hasta 8 invitados) y la tarjeta ONLINE del launcher, probados con Killer Instinct
entre dos casas.

## What Changes

- **Rollback con GekkoNet** (HeatXD/GekkoNet, BSD-2-Clause, C/C++20), fijado a un commit: la sesión guarda y carga el
  estado de la placa (`NeoGeo::Serialize`), avanza un cuadro con las entradas de los dos jugadores (`players[2]`,
  `RunFrame`) y compara `StateHash` para detectar desincronizaciones. Retraso de entrada y predicción ajustables.
- **La conexión por el túnel**: el juego usa el adaptador UDP de GekkoNet hacia `127.0.0.1`, y `cps-tunel` lleva ese
  tráfico como hoy lleva el de UltraCombo. Nada de puertos que abrir ni servidor. (Un adaptador propio que hable con el
  túnel sin UDP local queda para después, si hace falta.)
- **Línea de comandos**: `--host [PUERTO]` y `--join HOST:PUERTO`, el mismo contrato que UltraCombo, para que la tarjeta
  ONLINE arranque los juegos de Neo Geo igual que KI.
- **Espectadores**: un amigo entra a la sala a mirar, también a mitad de la pelea (GekkoNet carga un estado confirmado);
  nunca detiene ni termina la pelea de los dos jugadores.
- **Replays**: cada reta en línea se guarda (entradas, estado inicial, versión y juego, con los checksums por cuadro), se
  ve después y, sin ventana, sirve para hacer video.
- **Seguro contra desync**: antes de habilitar un juego en línea pasa la sesión de estrés de GekkoNet (rebobina una y
  otra vez en una sola PC); en línea se apagan los save states, la cámara lenta y los trucos; los dos lados comparan
  versión, juego y set de ROM antes de empezar.
- **Lecciones de SF4 Ember** (la misma separación juego + Iroh): la tolerancia de desconexión de la sesión queda por
  debajo de la inactividad del túnel (8 s) y cada paquete se trata como no confiable.

Fuera de alcance:
- Los juegos de Capcom (siguen el netplay de upstream, change `capcom-online` de ForkAde) y Killer Instinct (el de
  UltraCombo).
- Lobby, salas públicas y servidor propio.
- Más de dos jugadores.

## Capabilities

### New Capabilities
- `neogeo-netplay`: la sesión en línea de los juegos de Neo Geo: conectar como anfitrión o invitado por el túnel, el
  rollback, qué se apaga en línea, la verificación entre jugadores y la detección de desync.
- `neogeo-spectators-replays`: mirar una reta como espectador y guardar, ver y reproducir sin ventana las retas en línea.

### Modified Capabilities
<!-- Ninguna en openspec/specs. Depende de neogeo-board (change neogeo-boot, sin archivar) y de la ventana del port de
Neo Geo, que llega en un paso anterior a este. -->

## Impact

- **Orden**: después de `neogeo-boot` (la placa), la traducción a C++ y la ventana del port de Neo Geo; en línea sin
  ventana se puede probar antes, con dos `neogeo-run` y dos túneles, como se probó KI.
- **Dependencia nueva**: GekkoNet por FetchContent (con su ASIO para el adaptador UDP), solo en los juegos de Neo Geo.
- **Código**: el ciclo en línea del port de Neo Geo, un módulo de sesión, `games/kof97/game.cfg` (si está habilitado en
  línea).
- **Launcher (fork) y ForkAde**: la tarjeta ONLINE en los juegos de Neo Geo, con su puerto; el paquete y sus
  instrucciones.
