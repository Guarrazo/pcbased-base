# Estado del arte

Convención de este documento: cada afirmación va marcada como **[CONFIRMADO]** (verificado en fuentes públicas), **[INFERENCIA]** (deducción razonable a partir de hechos confirmados, sin verificación directa) o **[DESCONOCIDO]** (requiere ingeniería inversa o pruebas propias). No se inventa comportamiento no documentado.

## 1. Traducción binaria x86/x64 → ARM64

### Box64 / Box86 (ptitSeb)
- **[CONFIRMADO]** Box64 es un emulador de espacio de usuario para Linux que ejecuta binarios ELF x86-64 en hosts ARM64 (también RISC-V64 y LoongArch64), sin virtualización de SO completa: traduce llamadas de función y reexpone directamente librerías nativas del host (libc, libm, SDL, OpenGL) en vez de emularlas.
- **[CONFIRMADO]** Su DynaRec (recompilador dinámico) construye "dynablocks": bloques de código nativo ARM64 equivalentes a bloques de instrucciones x86, en cuatro pasadas (conteo de instrucciones → resolución de saltos/flags con un algoritmo tipo Kildall → generación → linkado de bloques). Dan un speedup de 5-10x frente al modo intérprete puro.
- **[CONFIRMADO]** Box64 solo traduce binarios de 64 bits. Box86 es el equivalente para binarios x86 de 32 bits y es el proyecto maduro; el soporte de 32 bits dentro de Box64 ("Box32") está marcado oficialmente como experimental y "prácticamente nada funciona todavía".
- **[CONFIRMADO]** Box64 gestiona el modelo de memoria x86 (más fuerte, TSO) sobre ARM64 (más débil) mediante protección de memoria + manejador de señales SIGSEGV, con varios niveles configurables de barreras de memoria (desde ninguna hasta emulación estricta de TSO al estilo QEMU).
- **[INFERENCIA]** Al depender de `mmap`/`mprotect`/manejadores de señales POSIX y de la presencia de librerías nativas equivalentes en el host (glibc, SDL, OpenGL), Box64 tal como está **no es portable directamente a Horizon OS**: Switch no tiene señales POSIX estilo Linux, no tiene glibc, y no expone OpenGL. Se puede reutilizar el **diseño del DynaRec y buena parte del decodificador de instrucciones x86**, pero no el runtime.

### FEX-Emu
- **[CONFIRMADO]** FEX también es un traductor binario dinámico x86/x86-64 → ARM64 para Linux, pero en vez de generar ARM64 directamente desde x86 decodificado, primero traduce a una **IR (representación intermedia) propia**, la optimiza, y genera ARM64 desde esa IR. Esto le da más margen para optimizaciones agresivas que un enfoque de traducción directa bloque a bloque.
- **[CONFIRMADO]** Soporta "thunking": en vez de emular una librería x86 (p. ej. Vulkan o GL), intercepta la llamada y la redirige a la librería nativa ARM64 del host — evita re-emular la superficie completa de una API gráfica.
- **[CONFIRMADO]** Al igual que Box64, gestiona la diferencia de modelo de memoria (x86 TSO vs ARM64 weak ordering) con barreras; casos límite pueden comportarse de forma distinta al x86 real.
- **[INFERENCIA]** El diseño con IR intermedia es **más adecuado como referencia arquitectónica para este proyecto** que el de Box64, precisamente porque una IR propia es el punto de enganche natural para: (a) inyectar hooks/patches a nivel de bloque en vez de a nivel de instrucción nativa, y (b) emitir código para un backend ARM64 "a medida" que respete las restricciones de Horizon OS (memoria JIT vía `jitCreate`, no `mmap(PROT_EXEC)`).

### Comparativa Box64 vs FEX
- **[CONFIRMADO]** Ambos son alternativas directas para el mismo problema (x86/x64 → ARM64 en espacio de usuario Linux); ninguno es objetivamente "mejor" en todos los casos — el rendimiento depende del juego/carga de trabajo. FEX se orienta más a gaming con extensiones modernas (SSE4.1, AVX/AVX2 en desarrollo); Box64 tiene una superficie de compatibilidad muy amplia y wrappers extensos para librerías Linux/Wine.
- **[CONFIRMADO]** Wine + FEX (o Wine + Box64) es la combinación estándar actual en 2026 para ejecutar juegos Windows x86/x64 en Linux ARM64 (p. ej. CrossOver ARM64, que integra FEX; Wine 11.0 estable de enero de 2026 ya incluye soporte inicial para módulos ARM64EC).

## 2. TeknoParrot: qué es y qué no es

- **[CONFIRMADO]** TeknoParrot es, textualmente, un **launcher específico por título y capa de mapeo de hardware**, no un emulador de propósito general. Corre de forma nativa sobre Windows x86/x64 real — los juegos son ejecutables Windows sin modificar (o mínimamente parcheados) que ya corren sobre la CPU x86 real de la máquina del usuario.
- **[CONFIRMADO]** No hay traducción de CPU en TeknoParrot: es Windows x86 ejecutando binarios x86 nativos. Su trabajo es sustituir la placa arcade (JVS I/O, dongles de seguridad, ALL.Net, sensores, etc.) por equivalentes software/hardware de PC.
- **[CONFIRMADO]** Cada juego tiene un perfil (XML/JSON) en una carpeta `GameProfiles`/`UserProfiles` que declara: ejecutable a lanzar, mapeo de controles, parches a aplicar, y metadatos de la placa arcade original.
- **[CONFIRMADO]** Usa proyectos satélite como Demulshooter para enrutar pistolas de luz/volantes a los IDs de dispositivo que cada juego espera.
- **Consecuencia directa para este proyecto [INFERENCIA]:** como TeknoParrot no resuelve el problema de CPU (asume x86 nativo), **no hay ningún componente de TeknoParrot que podamos "portar"** para la parte más cara del proyecto (traducción x86→ARM64). Lo que sí es trasladable conceptualmente es su modelo de **Game Profile + Patch Engine + virtualización de I/O arcade**, que es exactamente lo que se pide en las secciones 11-12 del encargo.

## 3. El precedente más relevante: Lindbergh Loader

Este es, con diferencia, el hallazgo más importante de la investigación para decidir el primer objetivo (ver `ROADMAP.md`).

- **[CONFIRMADO]** El proyecto **lindbergh-loader** (y su sucesor **linuxloader**) es una reimplementación open source, completamente vía ingeniería inversa, de la BIOS/placa base virtual de la Sega Lindbergh, para Linux x86.
- **[CONFIRMADO]** La Lindbergh **no usa Windows**: corre MontaVista Linux embebido. Los ejecutables de los juegos son **binarios ELF x86 de 32 bits** (no x86-64) enlazados contra librerías estándar de Linux más unas pocas librerías propietarias de Sega.
- **[CONFIRMADO]** El loader funciona como una librería compartida (`lindbergh.so`) que se precarga y que:
  - Interpone `open()`/`fopen()` para redirigir rutas de dispositivo (`/dev/lbb` = baseboard, `/dev/i2c/0` = eeprom, `/dev/ttyS0`/`/dev/ttyS1` = drive/ride/motion boards y lector de tarjetas, accesos a `/proc/bus/pci/...`) hacia código propio o hacia `/dev/zero`.
  - Sintetiza `eeprom.bin`/`sram.bin` en el primer arranque, calculando los CRC de las secciones `amSysDataRecord` a partir de constantes conocidas (no hay volcado real de keychip: la "placa de seguridad" son en realidad DIP switches y líneas test/service).
  - Resuelve el runtime gráfico de los juegos, que usan **NVIDIA Cg 3.1 sobre OpenGL** (no DirectX), enlazando `libCg.so`/`libCgGL.so` con comprobación de versión exacta.
  - Pasa la comunicación JVS (I/O arcade) a través de puerto serie real o la sintetiza.
- **[CONFIRMADO]** El proyecto **OpenJVS** confirma explícitamente que Lindbergh es una placa JVS "Working" y bien documentada, junto con Naomi, Chihiro, Namco System 23, Taito Type X, etc.
- **Implicación arquitectónica directa [INFERENCIA]:** para Lindbergh, la superficie a reproducir es **muchísimo más pequeña** que "un PC completo" o incluso que "Win32": son un puñado de rutas de fichero/dispositivo interceptadas, un formato eeprom/sram trivial, un runtime OpenGL+Cg, y JVS. **No hace falta ninguna capa Win32 en absoluto** para el primer objetivo — es la opción (B)/(C) del encargo, pero más reducida todavía: ni siquiera hay Windows de por medio, solo CPU x86 (32 bits) + libc mínima + gráficos + un par de dispositivos.

## 4. Gráficos: traducción de APIs

- **[CONFIRMADO]** DXVK traduce Direct3D 9/10/11 a Vulkan; VKD3D-Proton traduce Direct3D 12 a Vulkan; D8VK/dgVoodoo cubren DirectDraw/D3D7-8 antiguos. Todos asumen un backend Vulkan disponible en el sistema destino.
- **[CONFIRMADO]** Nintendo Switch homebrew **no expone Vulkan**: el único acceso GPU de bajo nivel disponible para homebrew es **NVN** (API privativa de Nintendo, envuelta para homebrew por **deko3d**, la librería de fincs que ya usas en `DLSS-Switch`). No existe un driver Mesa/OpenGL oficial para homebrew en Horizon OS.
- **Consecuencia directa [INFERENCIA]:** **DXVK/VKD3D no son reutilizables tal cual** en Switch porque su backend de salida (Vulkan) no existe en la plataforma. Sirven como **referencia de diseño** (cómo mapear estados de pipeline, formatos de textura, semántica de render targets) pero el backend de salida real tiene que generar comandos deko3d/NVN, no SPIR-V/Vulkan. Esto es coherente con tu trabajo ya hecho en DLSS-Switch (deko3d sobre Maxwell GM20B).
- **[INFERENCIA]** Para el objetivo Lindbergh, el problema es más acotado: no hace falta traducir Direct3D en absoluto, solo **OpenGL 1.x/2.x de función fija + shaders Cg** a deko3d. Esto es un subconjunto mucho más pequeño que "soportar Direct3D 9-12".

## 5. Windows en Switch (Win32/Wine)

- **[CONFIRMADO]** Wine es un proyecto enorme (C, ~30 años de desarrollo) pensado para SO tipo Unix con syscalls POSIX, `mmap` con permisos arbitrarios, señales, hilos POSIX, etc. Incluso su port a ARM64 (Linux/macOS) presupone un kernel Unix debajo.
- **[INFERENCIA]** Portar Wine completo a Horizon OS (que no tiene syscalls POSIX, no tiene `fork()`, tiene su propio modelo de memoria/IPC, y en homebrew impone W^X estricto sobre páginas ejecutables) es un proyecto de varios años-persona por sí solo, y no es lo que TeknoParrot necesita tampoco (TeknoParrot no reimplementa Windows, corre sobre Windows real). **Se descarta portar Wine para el MVP.** Ver `WINDOWS_COMPATIBILITY.md` para la alternativa (capa Win32 mínima, solo para el roadmap posterior a Lindbergh).

## 6. Plataforma Nintendo Switch (homebrew)

- **[CONFIRMADO, de tu propio trabajo en Super3-NX]** Horizon OS impone W^X (write XOR execute) en memoria de aplicación homebrew: no se puede `mmap`/`aligned_alloc` + `svcSetMemoryPermission` para obtener páginas RWX. El patrón correcto y ya validado por ti es `jitCreate()` → `jitTransitionToWritable()` (escribir código) → `jitTransitionToExecutable()` (ejecutar), del subsistema JIT de libnx.
- **[CONFIRMADO, de tu propio trabajo]** Ya tienes un decodificador+emisor de instrucciones ARM64 depurado a nivel de bit (encodings LDR/STR, BLR, LDP/STP) para `switch_ppc_jit_arm64.h` de Super3-NX. Ese emisor ARM64 es **reutilizable directamente** como backend de bajo nivel del nuevo JIT x86→ARM64: el problema de "cómo emitir ARM64 válido y ejecutarlo bajo Horizon OS" ya está resuelto una vez; lo que cambia es el decodificador de entrada (x86 en vez de PowerPC) y el mapeo de registros/flags.
- **[CONFIRMADO, de tu propio trabajo]** Ya conoces `so-loader` (técnica de compatibilidad binaria ARM64 para ports de Android) como precedente de "cargador que sustituye un runtime ajeno por shims propios" — es el mismo patrón conceptual que lindbergh-loader, aplicado a otro par origen/destino.

## 7. No verificado / requiere ingeniería inversa

- **[DESCONOCIDO]** Comportamiento exacto de cada juego Lindbergh individual más allá de lo que documenta públicamente `lindbergh-loader/docs/supported.md` — hay que probarlo binario por binario.
- **[DESCONOCIDO]** Si el compilador de shaders offline de deko3d (`uam`) puede integrarse en una toolchain de build para precompilar shaders Cg conocidos de un juego, o si hace falta un paso de traducción Cg→GLSL→uam manual por shader. Esto es un riesgo técnico real, ver `docs/GRAPHICS.md` y `docs/ROADMAP.md`.
- **[DESCONOCIDO]** Rendimiento real esperable: no existe ningún benchmark público de Box64/FEX-style DynaRec corriendo específicamente sobre Cortex-A57 a las frecuencias de Switch (esto exige medición propia, no asunción).
