# Spec Delta

## Purpose

El video LSPC del Neo Geo en el toolkit: la capa fija, los sprites en tiras verticales con encogimiento, las paletas y
el orden de dibujo, para que los juegos de Neo Geo se vean como en la placa, empezando por el arranque del BIOS y la
intro y la selección de KOF '97.

## ADDED Requirements

### Requirement: La imagen del MVS
El video SHALL dar una imagen de 320 x 224 puntos por cuadro (la parte visible del MVS), con el color de fondo de la
paleta donde no hay nada dibujado, y SHALL poder guardarse como PNG desde la prueba sin ventana, como en los juegos de
CPS.

#### Scenario: Captura sin ventana
- **WHEN** se corre KOF '97 sin ventana con capturas cada cierto número de cuadros
- **THEN** cada PNG mide 320 x 224 y tiene los colores de la paleta del juego

### Requirement: La memoria de video y sus registros
El video SHALL leer y escribir su memoria (las listas de sprites, sus tamaños y posiciones y el mapa de la capa fija)
por los registros de dirección, dato y avance del MVS, y SHALL tener las dos paletas de 4,096 colores que elige el
programa.

#### Scenario: El programa escribe en la memoria de video
- **WHEN** el programa pone una dirección, un avance y escribe palabras seguidas por el registro de dato
- **THEN** quedan en direcciones seguidas según el avance y se leen igual por el mismo registro

### Requirement: La capa fija
El video SHALL dibujar encima de todo la capa fija de 40 x 32 cuadros de 8 x 8 desde los gráficos S del cartucho o los
del sistema, según elija el programa, con su paleta por cuadro.

#### Scenario: El arranque del BIOS
- **WHEN** el BIOS arranca, antes de pasar al juego
- **THEN** sus textos y la pantalla del MVS se ven con la capa fija del sistema

### Requirement: Los sprites
El video SHALL dibujar los 381 sprites del MVS, cada uno una tira vertical de cuadros de 16 x 16 de los gráficos C, con
su posición, su altura, su encogimiento horizontal y vertical según la tabla del BIOS, el volteo de cada cuadro, la
paleta de cada cuadro y el encadenado ("sticky") que pone un sprite junto al anterior. La auto-animación de cuadros
SHALL avanzar como en la placa. El orden de dibujo SHALL ser el de la lista, con el límite de sprites por línea del
MVS.

#### Scenario: La intro de KOF '97
- **WHEN** KOF '97 corre su intro
- **THEN** los personajes y fondos se ven en su lugar, sin huecos ni cuadros cambiados, comparados con la misma escena
  en MAME

#### Scenario: Encogimiento
- **WHEN** una escena usa sprites encogidos (los acercamientos de la intro o de la selección)
- **THEN** se ven del tamaño que les da la tabla de encogimiento, sin líneas sueltas
