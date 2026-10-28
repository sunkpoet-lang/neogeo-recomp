# Spec Delta

## Purpose

El programa de un juego de Neo Geo con ventana: la imagen, la escala, la pantalla completa, los controles de dos
jugadores, la moneda y el ritmo de la placa, sin perder la prueba sin ventana que usan las comprobaciones.

## ADDED Requirements

### Requirement: La ventana
El programa SHALL abrir una ventana con la imagen de la placa (320 x 224) escalada para verse a 4:3, SHALL pasar a
pantalla completa y volver con F11, SHALL guardar una captura PNG con F12 y SHALL cerrarse con Esc o con la X de la
ventana.

#### Scenario: Abrir el juego
- **WHEN** el jugador abre `kof97-neogeo.exe` con sus archivos en su lugar
- **THEN** se abre la ventana, arranca el BIOS del MVS y luego el juego, con sonido

### Requirement: El ritmo de la placa
El juego SHALL correr a la velocidad de la placa (59.19 cuadros por segundo), sin acelerarse en una pantalla de más Hz
ni quedarse atrás en una de 60.

#### Scenario: Un minuto de juego
- **WHEN** el juego corre un minuto con ventana
- **THEN** corrió unos 3,551 cuadros y el sonido no se cortó ni se acumuló

### Requirement: Controles de dos jugadores
El programa SHALL leer el teclado y hasta dos controles: palanca, botones A-D, start y moneda de cada jugador, con un
acomodo de teclado fijo y documentado (jugador 1: flechas, A S para A y C, Z X para B y D, 1 start, 5 moneda; jugador
2: I J K L, T Y G H, 2 start, 6 moneda) y el acomodo usual de un control. Una pulsación breve de moneda SHALL contar un
crédito.

#### Scenario: Una moneda de un toque
- **WHEN** el jugador toca la tecla de moneda una vez, aunque sea un instante
- **THEN** el juego cuenta un crédito, ni cero ni dos

#### Scenario: Dos controles
- **WHEN** hay dos controles conectados
- **THEN** el primero es el jugador 1 y el segundo el jugador 2

### Requirement: Sin ventana como antes
Con `--headless`, el programa SHALL comportarse como `neogeo-run` de `neogeo-boot` (capturas, `--hash-log`, `--script`,
estados), sin abrir ventana ni dispositivo de sonido.

#### Scenario: Las comprobaciones siguen
- **WHEN** se corre con `--headless --frames 3000 --script ...`
- **THEN** no se abre ninguna ventana y el hash del cuadro 3,000 es el de una corrida igual
