# Patch Engine

## Alcance soportado

- **Byte patches**: sustitución de bytes en el binario original antes de decodificar (offset, bytes originales esperados como comprobación de seguridad, bytes nuevos).
- **NOP / instruction replacement**: caso particular del anterior a nivel de instrucción decodificada, no de bytes crudos — evita romper la traducción si el offset de bytes cambia por una revisión del binario pero la instrucción semántica no.
- **Memory patches en tiempo de ejecución**: escritura tras la carga, útil para constantes que el juego calcula en vez de tener como dato estático.
- **Function/API hooks**: intercepción por nombre de símbolo (para funciones de la libc-shim, Cg, GL) — implementados como entradas en la tabla de resolución de símbolos del cargador ELF, no como trampolines sobre código ya traducido.
- **Import/export hooks**: reservado para el roadmap Windows (tabla de imports PE), no aplica a ELF de la misma forma.
- **Trampolines/detours a nivel de bloque IR**: para parchear código ya traducido (p. ej. saltarse una comprobación de timing/versión), se inserta el patch como una transformación sobre la IR del bloque afectado (ver `docs/CPU_TRANSLATION.md`) — nunca parcheando bytes ARM64 ya emitidos directamente, para que la invalidación/recompilación de bloques siga funcionando de forma consistente.

## Por qué a nivel de IR y no de bytes ARM64 finales

Un patch sobre bytes ARM64 ya generados se rompe en cuanto cambia cualquier detalle de la generación de código (reordenación de instrucciones, un cambio en el asignador de registros). Un patch expresado como transformación de IR sobrevive a esos cambios porque se reaplica cada vez que el bloque se (re)traduce. Esto es coherente con la decisión de usar IR propia documentada en `CPU_TRANSLATION.md`.

## Formato de datos (parte de un `GameProfile`)

```jsonc
"patches": [
  { "type": "byte", "offset": "0x10a4", "expect": "7402", "replace": "9090" }
],
"hooks": [
  { "type": "symbol", "symbol": "check_security_dongle", "action": "stub_return_ok" }
]
```

Ningún patch se aplica "a ciegas": el campo `expect` (bytes originales esperados) es obligatorio para byte patches — si no coincide, el motor rechaza el patch y lo registra en el log en vez de aplicarlo sobre un binario que no es el esperado (evita corrupciones silenciosas si alguien usa una revisión distinta del ELF).

## Eficiencia en ARM64

El motor de patches no debe introducir una indirección extra en el camino caliente de ejecución cuando un bloque **no** tiene patches — el chequeo de "¿este bloque tiene patches?" se resuelve una vez, en tiempo de traducción (al construir la IR), no en cada ejecución del bloque.
