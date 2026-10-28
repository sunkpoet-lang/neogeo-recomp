# neogeo-port-frontend Specification

## Purpose
El programa de un juego de Neo Geo con las piezas que comparten los ports de la colección: el menú del juego, los
ajustes, los controles del launcher, los save states, el presentador con sus shaders, el HUD, y las fuentes del juego
donde se enchufan después la música propia y el modo de entrenamiento.

## Requirements

### Requirement: El menú del juego
El programa SHALL abrir el menú de los ports (Esc, F1, el clic del stick izquierdo, Start y Select juntos), detener el
juego mientras está abierto, y SHALL ofrecer save states por ranura, moneda, velocidad, sonido, las features y los
comandos del juego, y salir.

#### Scenario: Un save state desde el menú
- **WHEN** el jugador guarda en una ranura desde el menú, sigue jugando y luego la carga
- **THEN** el juego vuelve a donde se guardó, con su sonido

#### Scenario: El menú sin ventana
- **WHEN** se corre sin ventana con `--menu` y teclas para guardar y cargar
- **THEN** el estado después de cargar tiene el hash del de cuando se guardó

### Requirement: Los controles del launcher
El programa SHALL leer de su `settings.ini` los controles que guarda el launcher (teclas y botones de control de cada
jugador), con la tabla compartida si el juego no tiene una propia, y SHALL leer hasta dos controles.

#### Scenario: Botones cambiados en el launcher
- **WHEN** el jugador cambia en Controllers qué botón da A
- **THEN** en el juego ese botón da A

### Requirement: Teclas mientras se juega
El programa SHALL tener las teclas de los ports de Capcom: P para detener y seguir, O cuadro a cuadro, - y = la
velocidad, F5 y F9 guardar y cargar, F6 y F7 la ranura, F2 el switch de prueba del operador, F11 pantalla completa,
F12 la música propia cuando la haya.

#### Scenario: Cuadro a cuadro
- **WHEN** el jugador detiene con P y aprieta O tres veces
- **THEN** el juego avanza tres cuadros y sigue detenido

### Requirement: La imagen y el HUD
El programa SHALL mostrar la imagen con el presentador compartido (los shaders, el escalado y la forma de la página
Display, con el 4:3 del MVS) y SHALL dibujar encima las líneas y cajas que pidan las fuentes del juego.

#### Scenario: Un shader
- **WHEN** el jugador elige un shader CRT en la página Display
- **THEN** KOF '97 se ve con ese shader

### Requirement: Las fuentes de un juego de Neo Geo
Un juego de Neo Geo SHALL poder tener fuentes propias (`games/<id>/src`) que declaren features y comandos, actúen antes
y después de cada cuadro con la placa, pongan líneas y cajas en el HUD y guarden su propio estado en los save states,
sin cambiar la placa.

#### Scenario: Una feature de prueba
- **WHEN** las fuentes de KOF '97 declaran una feature y el jugador la enciende
- **THEN** aparece en el menú y en la página Features, y su efecto se ve en el juego

### Requirement: Sin ventana como antes
Con `--headless`, el programa SHALL seguir dando lo mismo que en `neogeo-window-sound` (capturas, `--hash-log`,
`--script`, estados, `--audio-dump`), con los mismos hashes en dos corridas iguales.

#### Scenario: Las pruebas siguen
- **WHEN** se corre sin ventana 3,000 cuadros con la misma `--script`
- **THEN** dos corridas dan `--hash-log` idénticos
