# Tasks

## 1. El YM2610

- [x] 1.1 Agregar `third_party/ymfm_opn.*`, `ymfm_ssg.*` y `ymfm_adpcm.*` del commit 81aec25 sin cambios, compilados con `-w`, y su aviso en `THIRD_PARTY_NOTICES.md`. Verificar con `git diff` contra el commit que son idénticos y que los juegos de Capcom compilan igual.
- [x] 1.2 `machine/neogeo_sound.*`: el `ym2610` con su `ymfm_interface` (samples V para ADPCM-A y B, temporizadores en ciclos del Z80, la línea de interrupción) y la mezcla de sus salidas. Verificar con una corrida sin ventana que las escrituras del Z80 llegan al chip y que no quedan accesos sin mapear en sus puertos.
- [x] 1.3 Conectarlo a `NeoGeo` en lugar del YM2610 mudo: puertos 0x04-0x07, temporizadores revisados al correr el Z80, muestras generadas línea por línea, estado en `Serialize`. Verificar que dos corridas de 3,000 cuadros dan `--hash-log` idénticos (escenario "Dos corridas con sonido") y que guardar y cargar a mitad sigue igual.
- [x] 1.4 `--audio-dump FILE` y `--mute` sin ventana. Verificar que el WAV de la intro tiene la música (se escucha el archivo y se mide su largo: los cuadros entre 59.19) y que `--mute` da los mismos hashes.

## 2. La ventana

- [x] 2.1 `port/neogeo_window.cpp`: ventana SDL3, textura de 320 x 224 a 4:3, F11, F12, Esc y la X. Verificar que compila y que `--headless` no abre ventana ni sonido (escenario "Las comprobaciones siguen").
- [x] 2.2 El sonido por `SDL_OpenAudioDeviceStream` y el ritmo por la cola (decisión 3). Verificar con un registro de cuántos cuadros corren en 60 s (unos 3,551) en una corrida que el usuario abre.
- [x] 2.3 Controles (decisión 4): teclado fijo de dos jugadores, dos controles, la moneda de 40 cuadros. Verificar sin ventana con una `--script` de moneda de 1 cuadro que cuenta un crédito, y con el usuario el teclado y su control.
- [x] 2.4 Un programa por juego (decisión 5): el objetivo `kof97`, sin `--rom` busca `roms/kof97.zip` junto a él; `make neogeo-run GAME=kof97`. Verificar desde otra carpeta con `roms/` al lado.

## 3. El launcher

- [x] 3.1 `launcher/src/description.cpp`: el programa del port de Neo Geo (carpeta `NeoGeo`, archivos en `roms`, su texto de controles) y el juego `kof97` (`kof97.zip`, `neogeo.zip`). Verificar con la prueba del launcher sin ventana (`--screenshot`) que KOF '97 aparece y que la página dice qué falta cuando no está `neogeo.zip`.
- [x] 3.2 El puente `kof97` en `launcher/CMakeLists.txt` y un icono propio sencillo (no el logo de SNK). Verificar que el puente abre el programa con doble clic (como se probó con KI) y que tiene su icono.
- [x] 3.3 Instalar en una copia de prueba del launcher (`NeoGeo/kof97.exe` y `NeoGeo/roms/` con los archivos del usuario) y que el usuario la abra: palomita, Play, sonido, controles y acceso directo. Verificar con lo que el usuario dice que ve y oye.

## 4. Comprobaciones

- [x] 4.1 Los juegos de Capcom: la línea base de `neogeo-boot` (SF2 CE y Super Turbo, 3,000 cuadros) con la rama terminada. Verificar hashes idénticos.
- [x] 4.2 KOF '97 sin ventana: arranque, intro, crédito de un toque y selección de equipo, con capturas y WAV. Verificar las capturas y mostrarlas al usuario.
- [x] 4.3 Actualizar el README de `games/kof97` (cómo abrirlo, controles, dónde van los archivos) y el `THIRD_PARTY_NOTICES.md`. Verificar que nombran el programa por su nuevo nombre.
