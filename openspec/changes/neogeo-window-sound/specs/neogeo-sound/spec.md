# Spec Delta

## Purpose

El sonido de la placa Neo Geo en el toolkit: el YM2610 que toca el programa del Z80 (FM, SSG, ADPCM-A y ADPCM-B), sus
temporizadores y la interrupción que dan al Z80, de modo que los juegos de Neo Geo suenen como en la placa sin dejar
de ser repetibles.

## ADDED Requirements

### Requirement: El YM2610 suena
La placa SHALL tener un YM2610 a 8 MHz que el Z80 programa por sus puertos 0x04-0x07, con sus canales FM, SSG,
ADPCM-A (los samples V del cartucho) y ADPCM-B, y SHALL dar su salida estéreo a la frecuencia propia del chip.

#### Scenario: La música de la intro
- **WHEN** KOF '97 corre su intro
- **THEN** su música y sus voces suenan, sin silencios ni ruido donde la placa no los tiene

#### Scenario: Grabación sin ventana
- **WHEN** se corre sin ventana con `--audio-dump` un tramo con música
- **THEN** el WAV tiene la música del juego, del largo de los cuadros corridos

### Requirement: Temporizadores e interrupción del Z80
Los temporizadores A y B del YM2610 SHALL contar como en el chip y, cuando el programa los habilita, SHALL subir la
línea de interrupción del Z80 hasta que el programa los atiende.

#### Scenario: El tempo de la música
- **WHEN** el programa de sonido usa un temporizador para su tempo
- **THEN** la música va al tiempo de la placa (un minuto de juego son unos 3,551 cuadros)

### Requirement: El sonido no cambia el juego
Con sonido, una corrida SHALL dar los mismos hashes de estado en cada cuadro en dos corridas iguales, y el sonido SHALL
poder apagarse (`--mute`) sin cambiar lo que hace el juego.

#### Scenario: Dos corridas con sonido
- **WHEN** se corre KOF '97 dos veces 3,000 cuadros con la misma `--script`
- **THEN** los `--hash-log` son idénticos
