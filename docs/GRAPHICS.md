# Arquitectura gráfica

## Decisión

**Backend de salida: deko3d (envoltorio homebrew de NVN), no Vulkan.** Switch homebrew no expone Vulkan (ver `STATE_OF_THE_ART.md`), así que DXVK/VKD3D/D8VK no son utilizables como backend final — sirven solo como referencia de diseño de la capa de traducción (mapeo de estados, formatos, semántica), no como código a integrar.

Para el MVP (Lindbergh, ver `ROADMAP.md`) el problema real es más pequeño de lo que sugiere el encargo original, porque **Lindbergh usa OpenGL + NVIDIA Cg 3.1, no Direct3D**:

```text
Juego (llamadas OpenGL 1.x/2.x función fija + shaders Cg)
        ↓
libGL-shim (nuestra, ARM64) — misma técnica que "thunking" de FEX:
intercepta símbolos glXxx() y cgXxx() en vez de reemular la ISA que los llama
        ↓
Traductor de estado GL → deko3d (pipeline, texturas, render targets, blending)
        ↓
Traductor de shaders Cg → shaders deko3d (DKSH)
        ↓
deko3d / NVN / Maxwell GM20B
```

## Por qué esto es más tratable que "Direct3D 9-12"

- El runtime de Lindbergh que espera cada juego es una versión fija y conocida de NVIDIA Cg 3.1 (`libCg.so`/`libCgGL.so`, comprobación de versión exacta `"2.0.0.12"` documentada por `lindbergh-loader`), no una superficie completa de Direct3D con años de extensiones.
- OpenGL de función fija (pipeline fijo, sin shaders) es un modelo de estado mucho más simple de mapear a un pipeline programable moderno (deko3d) que la variedad de generaciones de Direct3D.
- No hay que resolver el problema general "cualquier juego Direct3D 9-12" para el MVP — ese problema se aplaza al roadmap posterior, cuando se aborden sistemas Windows (RingEdge en adelante), y en ese momento sí conviene revisar en detalle el diseño de DXVK/vkd3d como referencia de mapeo de estados (no de código, por el mismo motivo que Wine: asume un backend que no tenemos).

## Riesgo técnico identificado: compilación de shaders en tiempo de ejecución

**[DESCONOCIDO] — riesgo abierto, no resuelto por esta investigación.** deko3d normalmente consume shaders **precompilados offline** con la herramienta `uam` (formato `.dksh`), no fuente en tiempo de ejecución. Los shaders Cg de un juego Lindbergh, en cambio, se compilan en tiempo de ejecución sobre la placa original (el runtime Cg compila a un profile tipo `arbvp1`/`arbfp1` sobre la marcha).

Dos caminos posibles, ninguno verificado todavía, a evaluar en la fase de prototipo del MVP:

1. **Precompilación por perfil de juego**: extraer offline (una vez, por juego, como parte de crear el `GameProfile`) el conjunto de shaders Cg reales que usa el binario, traducirlos a GLSL-equivalente y compilarlos con `uam` como parte del build/instalación del perfil. Esto encaja de forma natural con el sistema de perfiles (sección 11 del encargo) pero implica que **añadir un juego no es solo "copiar el ELF"**, sino un paso de extracción de shaders por juego.
2. **Traductor Cg→IR→DKSH en tiempo de ejecución**, escrito a medida (Cg no es un lenguaje enorme; sería viable escribir un compilador reducido para el subconjunto realmente usado). Coste de desarrollo mayor, pero elimina el paso de preparación manual por juego.

Se recomienda **empezar por (1)** para el MVP porque reduce drásticamente el alcance inicial, y dejar (2) como mejora de roadmap si el número de juegos objetivo crece.

## Consideraciones de rendimiento (Tegra X1 / Maxwell GM20B)

- Lección ya extraída por ti en Super3-NX: el cuello de botella no es necesariamente CPU sino **overhead de draw call y cambios de estado de textura** (75-80% del tiempo en binds de textura en tu perfilado). El traductor GL→deko3d debe **agrupar por material/textura** en vez de traducir 1:1 cada `glDrawXxx` del juego, exactamente la misma optimización que ya identificaste como de mayor ROI.
- El ancho de banda de memoria (25.6 GB/s) es más limitante que el cómputo de shaders en este hardware — conviene mantener formatos de textura comprimidos (evitar reconvertir a formatos sin comprimir salvo que el hardware no soporte el formato original) y evitar copias de framebuffer innecesarias en la capa de traducción.

## Interfaz interna (ver `src/graphics/`)

- `src/graphics/gl_shim/`: superficie de funciones `gl*`/`cg*` interceptadas (extensible por perfil de juego — un juego puede necesitar solo un subconjunto).
- `src/graphics/switch/`: backend real deko3d — el único que sabe de NVN/Maxwell.
- `src/graphics/d3d_shim/`: placeholder para el roadmap Windows, no se implementa en el MVP.

La frontera entre "traductor de estado" (independiente de plataforma) y "backend deko3d" (específico de Switch) es intencional — ver `docs/ARCHITECTURE.md`, separación `core`/`platform`.
