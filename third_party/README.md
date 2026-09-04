# third_party/

Nada vendorizado todavía en este esqueleto. Candidatos identificados durante
la investigación (ver `docs/`), a añadir cuando el Nivel 1 del MVP
(`docs/ROADMAP.md`) los necesite de verdad, no de forma especulativa:

| Dependencia | Para qué | Licencia a verificar antes de vendorizar |
|---|---|---|
| ~~Una librería JSON de cabecera única (p.ej. nlohmann/json)~~ | ~~Parseo de `GameProfile`/`PlatformProfile`~~ | **Ya no hace falta** — se implementó un parser JSON propio y pequeño (`src/profiles/json_lite.h`/`.cpp`, con tests en `tests/test_json_lite.cpp`) precisamente porque no había forma de vendorizar una librería con garantías en el entorno donde se generó este esqueleto. Sigue siendo *reemplazable* si en algún momento conviene una librería más completa (unicode completo, JSON Schema, etc.) — la superficie que toca el resto del código es `json::Value::AsString()`/`AsInt()`/`Get()`, no el parser en sí. |
| Referencia de `lindbergh-loader` (no código, ver más abajo) | Layout de `eeprom.bin`/`sram.bin` (`src/arcade/devices/eeprom.cpp`, TODO explícito) | El proyecto es GPL y además su propio README pide no reutilizar el código en ventas/suscripciones comerciales — para un proyecto de preservación open source esto no debería ser un problema, pero **revisa los términos exactos del proyecto antes de copiar código literal**; el *formato* de datos en sí (resultado de ingeniería inversa pública) es lo que se referencia en `docs/ARCADE_HARDWARE.md`, no necesariamente su implementación palabra por palabra |
| `switch_ppc_jit_arm64.h` (de tu propio proyecto Super3-NX) | **Recibido y ya explotado**: se contrastó formula a formula contra la implementación propia (coinciden bit a bit en todo lo comprobado, ver `docs/JIT.md`) y se portaron instrucciones nuevas que no existían aquí todavía (MOVZ/MOVK/MOVN, ADD/SUB/CMP inmediato, AND/ORR/EOR, LSL/LSR/ASR, MUL/UDIV/SDIV). Pendiente de una pasada futura: su capa `ValidateOpcode()` (validación estructural por familia de encoding) y sus registros tipados `XReg`/`WReg` son mejoras de diseño que vale la pena adoptar más adelante | Tuyo — sin problema de licencia |

No se ha vendorizado nada automáticamente en este commit porque el entorno donde se generó este esqueleto no tenía acceso de red a los repositorios necesarios para hacerlo con las versiones correctas — hazlo tú en tu máquina de desarrollo con `git submodule` o copiando el header correspondiente.
