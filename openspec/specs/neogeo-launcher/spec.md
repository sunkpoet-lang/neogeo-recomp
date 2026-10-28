# neogeo-launcher Specification

## Purpose
Los juegos de Neo Geo del fork en el launcher de la colección: dónde van su programa y sus archivos, cómo los reconoce
el launcher y cómo se abren, igual que los juegos que se juegan en un programa propio (Killer Instinct, 3rd Strike).

## Requirements

### Requirement: KOF '97 en la colección
El launcher SHALL mostrar The King of Fighters '97 entre sus juegos, con su programa (`kof97-neogeo.exe`) en una carpeta
`NeoGeo` junto al launcher (o hasta dos carpetas arriba, como UltraCombo) y sus archivos en `NeoGeo/roms`.

#### Scenario: Archivos completos
- **WHEN** `NeoGeo/roms` tiene `kof97.zip` y `neogeo.zip`
- **THEN** el juego tiene la palomita verde y "Play" lo abre en su ventana

#### Scenario: Falta el BIOS
- **WHEN** está `kof97.zip` pero no `neogeo.zip`
- **THEN** el juego no tiene la palomita y su página dice que falta `neogeo.zip`

### Requirement: Abrir desde el launcher y desde el escritorio
El juego SHALL abrirse con "Play" y con su puente `kof97` (el que usa el acceso directo al escritorio), con su propio
icono, y su programa SHALL encontrar sus archivos en `roms` junto a él sin que se le diga dónde.

#### Scenario: Acceso directo
- **WHEN** el jugador agrega el acceso directo de KOF '97 y le da doble clic
- **THEN** el juego abre directo, sin pasar por el launcher

### Requirement: Los demás juegos sin cambios
Los juegos de Capcom, Killer Instinct y 3rd Strike SHALL seguir apareciendo y abriéndose como antes.

#### Scenario: La colección completa
- **WHEN** se abre el launcher con todos los juegos instalados
- **THEN** los 10 de antes siguen con su palomita y KOF '97 aparece como el once
