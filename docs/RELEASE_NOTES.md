## Orbit Echo 1.1.0

**Novedad en 1.1.0:** interfaz rediseñada en estilo plano de colores cálidos (perilla DELAY con anillos,
secciones por color, barra de texto MODE / STYLE / ERA / PRESETS). El sonido, los parámetros y los presets
no cambian: los proyectos guardados con 1.0.0 se abren igual.

Delay estéreo multimodo (cinta, BBD, digital, pitch, frequency shift, phaser) con toda la coloración
dentro del lazo de retroalimentación. VST3 · AU · CLAP · Standalone para Windows, macOS (universal) y Linux.

### Descargas

| Sistema | Archivos |
|---|---|
| Windows (x64) | `OrbitEcho-Windows-VST3.zip`, `OrbitEcho-Windows-CLAP.zip`, `OrbitEcho-Windows-Standalone.zip` |
| macOS (Apple Silicon + Intel, 10.13+) | `OrbitEcho-macOS-VST3.zip`, `OrbitEcho-macOS-AU.zip`, `OrbitEcho-macOS-CLAP.zip`, `OrbitEcho-macOS-Standalone.zip` |
| Linux (x64) | `OrbitEcho-Linux-VST3.zip`, `OrbitEcho-Linux-CLAP.zip` |

**FL Studio (Windows):** copia `Orbit Echo.vst3` a `C:\Program Files\Common Files\VST3\` y usa
*Options → Manage plugins → Find installed plugins*. **macOS:** copia a `~/Library/Audio/Plug-Ins/…`
y, si el sistema lo bloquea, `xattr -dr com.apple.quarantine <ruta del plugin>`. Instrucciones para
cada DAW en el README.

### Incluye

- 17 modos × 5 styles × 3 eras, delay 0 ms – 20 s con sync, feedback hasta 200 % (siempre acotado).
- Drive con compensación, Age por modo, difusión calibrada (68 % campana, 91 % reverb), EQ en el lazo,
  Duck global, Width, Mix equal-power con Mix Lock.
- Interfaz original redimensionable (75–200 %), visualizador de ecos, 28 presets de fábrica, presets
  de usuario y A/B.
- Latencia 0, sin asignaciones de memoria en el hilo de audio, salida idéntica con cualquier buffer.

### Validación de esta versión

Compilado y probado en CI en Windows (MSVC 2022), macOS y Linux: tests Catch2 (los 13 del documento
de diseño + simulación de hosts + UI), pluginval estrictez 10 (VST3 y AU), validador VST3 de
Steinberg, `auval -strict`, clap-validator y un host de pruebas que carga el binario compilado.

Licencia AGPL-3.0 (JUCE 8). El uso comercial con código cerrado requiere licencia comercial de JUCE.
