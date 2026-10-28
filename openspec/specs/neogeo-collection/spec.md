# neogeo-collection Specification

## Purpose
Los juegos de Neo Geo del fork como juegos de la colección del launcher, igual que los de Capcom: dónde van sus sets,
cómo se reconocen, cómo se abren, dónde guardan sus ajustes, y la tabla de controles que comparten con los demás.

## Requirements

### Requirement: Un juego más de la colección
El launcher SHALL mostrar los juegos de Neo Geo entre los de la colección, con su página (Play, acceso directo,
Display, Features, Controllers) y su programa `<id>.exe` junto al launcher, como los de Capcom.

#### Scenario: KOF '97 en la colección
- **WHEN** el launcher abre con `kof97.exe` a su lado
- **THEN** The King of Fighters '97 aparece como un juego más, con las mismas secciones de página que Super Turbo

### Requirement: El set y el BIOS
El launcher SHALL reconocer el set de un juego de Neo Geo en la carpeta de ROMs (o el que el jugador elija), comprobando
por CRC el cartucho y el BIOS (`neogeo.zip`, junto al set), y SHALL decir cuál falta o cuál no es el esperado.

#### Scenario: Set y BIOS presentes
- **WHEN** la carpeta de ROMs tiene `kof97.zip` y `neogeo.zip`
- **THEN** KOF '97 tiene la palomita verde

#### Scenario: Falta el BIOS
- **WHEN** está `kof97.zip` pero no `neogeo.zip`
- **THEN** la página de KOF '97 dice que falta `neogeo.zip` junto al set

### Requirement: Sus ajustes y guardados
Un juego de Neo Geo SHALL guardar sus ajustes en su `settings.ini` y sus save states en su carpeta de la colección,
como los de Capcom, y SHALL recibir del launcher los mismos argumentos (`--rom`, `--settings`, `--features`,
`--fullscreen`).

#### Scenario: Una feature cambiada en el launcher
- **WHEN** el jugador cambia una feature de KOF '97 en el launcher y le da Play
- **THEN** el juego arranca con esa feature como quedó

### Requirement: La tabla de controles compartida
KOF '97 SHALL declarar 2 jugadores y sus botones A, B, C y D, y SHALL tomar sus controles de la página Controllers.
En la tabla que comparten los juegos de la colección, A y C SHALL caer en los dos primeros botones de arriba y B y D
en los dos primeros de abajo de un stick de seis botones; el jugador SHALL poder darle a KOF un acomodo propio.

#### Scenario: Un stick ya configurado
- **WHEN** el jugador ya configuró su stick para los juegos de Capcom
- **THEN** en KOF '97 la fila de arriba da A y C y la de abajo B y D, sin configurar nada más

### Requirement: Los demás juegos sin cambios
Los juegos de Capcom, Killer Instinct y 3rd Strike SHALL seguir apareciendo, reconociéndose y abriéndose como antes.

#### Scenario: La colección completa
- **WHEN** se abre el launcher con todos los juegos instalados
- **THEN** los diez de antes siguen igual y KOF '97 aparece como uno más
