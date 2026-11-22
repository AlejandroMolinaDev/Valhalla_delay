# Pruebas en DAWs (checklist manual)

Lo que se verifica **automáticamente en cada build** (Windows, macOS y Linux reales en GitHub Actions):

| Capa | Qué cubre |
|---|---|
| `tests/plugin/HostSimTests.cpp` | Patrones de FL Studio y otros hosts sobre el procesador: bloques de 0, 1, 7, 513, 4096, 8192 muestras y **mayores que los anunciados**, `prepareToPlay` repetido con 44,1 / 48 / 96 / 192 kHz, `processBlock` antes de `prepare`, estado leído/escrito desde otro hilo mientras suena (historial de deshacer de FL), bypass sin clicks, play head nulo / sin tempo / con tempo cambiante, **0 asignaciones de memoria** en el hilo de audio, ida y vuelta texto↔valor de cada parámetro, buses mono→mono, mono→estéreo y estéreo→estéreo. |
| `tools/HostHarness.cpp` | Carga el **binario compilado** (.vst3; en macOS también .component) a través del wrapper real, como un DAW: identidad, sin MIDI (sin parámetros CC fantasma en FL), 36 parámetros únicos y con nombre, latencia 0, cola declarada, bloques variables y de 8192, cambios de sample rate, parámetros por texto, **eco exacto a 250 ms a través del wrapper**, restauración del estado en una segunda instancia, estado durante el audio y bypass del host. |
| pluginval (estrictez 10), validador VST3 de Steinberg, `auval -strict`, clap-validator | Conformidad de los formatos VST3, AU y CLAP, editor abierto/cerrado repetidamente, fuzzing de parámetros, cambios de layout. |

Lo que **solo se puede confirmar a mano** en cada DAW está abajo. Lleva 10–15 minutos por host.

## FL Studio (Windows y macOS) — host principal

Preparación: instala según el README y haz *Options → Manage plugins → Find installed plugins*.

- [ ] **Escaneo:** *Orbit Echo* aparece como efecto (fabricante *Alejandro Audio*) en VST3 (y en CLAP con FL 2024+; en Mac también AU). Sin avisos en el log del escaneo.
- [ ] **Carga** en una ranura del Mixer; la ventana abre a 900×480 y se puede redimensionar desde la esquina (75–200 %). Con escalado de Windows al 125/150 % se ve nítida y proporcionada.
- [ ] **Audio:** con un loop sonando, cambia Mode, Style y Era: sin clicks ni silencios.
- [ ] **Sync:** pon *Sync* en 1/4 y cambia el tempo del proyecto: los ecos siguen el tempo y deslizan sin cortes.
- [ ] **Automatización:** clic derecho en un control → *Create automation clip*; reproduce: el control se mueve. *Browse parameters* muestra nombres claros (Delay, Feedback, Mode…) y valores con unidades.
- [ ] **Guardar / reabrir proyecto:** todos los valores, el preset, Mix Lock y el tamaño de la ventana vuelven igual.
- [ ] **Deshacer (Ctrl+Z)** tras mover controles: vuelve al estado anterior sin glitches.
- [ ] **Render** (*File → Export → WAV*) con Feedback alto: la cola no se corta al final.
- [ ] **Smart Disable** activado (en el menú del wrapper del plugin → *Smart disable*, o *Tools → Macros → Switch smart disable for all plugins*): tras parar, el plugin se duerme cuando los ecos terminan y se despierta al volver a sonar, sin cortar el primer eco.
- [ ] **Buffers:** cambia el buffer de audio (64 → 2048) y la opción *Use fixed size buffers* del wrapper del plugin: sin cambios en el sonido.
- [ ] **Multinúcleo:** varias instancias en distintas pistas con *Multithreaded mixer processing* activado: sin crujidos.
- [ ] **Bypass** de la ranura del Mixer: pasa la señal seca, el wet se apaga suavemente.
- [ ] **Presets:** navegación ◀ ▶, menú por categorías, *Save*, A/B y Mix Lock.
- [ ] **Mac:** si macOS bloquea el plugin, `xattr -dr com.apple.quarantine …` (README) y reescanear.

## Ableton Live (Windows / macOS)

- [ ] Aparece en *Plug-Ins → VST3* (y *Audio Units* en Mac). Carga en una pista de audio y en un Return.
- [ ] Automatiza Mix y Feedback desde el Arrangement; *Configure* muestra los parámetros.
- [ ] Freeze / Flatten de la pista con Feedback alto conserva la cola.
- [ ] Cambia el sample rate en *Preferences → Audio* mientras está cargado.

## Reaper (Windows / macOS / Linux)

- [ ] VST3 y CLAP aparecen tras *Re-scan*. Carga ambos en pistas distintas.
- [ ] *Render* con *Tail* activado: la cola completa.
- [ ] Cambia *Block size* y activa *Anticipative FX*: sin diferencias audibles.

## Bitwig Studio

- [ ] CLAP y VST3 aparecen. La modulación de parámetros desde los moduladores de Bitwig funciona (CLAP).
- [ ] Con *Plug-in hosting: Individually*, cerrar y reabrir el proyecto restaura el estado.

## Studio One / Cubase (Windows / macOS)

- [ ] Aparece en la categoría *Delay* (VST3 `Fx|Delay`).
- [ ] Inserción en pista mono: funciona (mono→estéreo / mono→mono).
- [ ] Bypass del host y automatización de lectura/escritura.

## Logic Pro / GarageBand (macOS, AU)

- [ ] Tras copiar el `.component` y reiniciar Logic, el *Plug-in Manager* lo marca como validado.
- [ ] Carga en pista mono (*Mono → Stereo*) y estéreo.
- [ ] GarageBand: aparece en *Plug-ins → Audio Units → Alejandro Audio* y abre su ventana.

---

Si algo falla, anota DAW + versión, sistema, formato (VST3/AU/CLAP), buffer/sample rate y los pasos,
y abre un *issue* en el repositorio.
