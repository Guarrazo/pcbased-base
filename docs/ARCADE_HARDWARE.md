# Hardware arcade: qué hay que virtualizar

## Separación por origen (según pide la sección 4 del encargo, aplicada al target Lindbergh)

| Categoría | Elemento | Quién lo provee en el MVP |
|---|---|---|
| A. Hay que emular/reimplementar | Formato eeprom/sram (constantes + CRC de `amSysDataRecord`) | `src/arcade/devices/eeprom.cpp` — síntesis igual que hace `lindbergh.so`, no hace falta volcado real |
| A. Hay que emular/reimplementar | Protocolo JVS (coin, switches, ejes analógicos) | `src/arcade/jvs/` — mapeado a HID de Switch (Joy-Con/Pro Controller vía libnx), no a un bus RS485 real |
| B. Hay que traducir | Llamadas OpenGL/Cg | `src/graphics/` (ver `GRAPHICS.md`) |
| C. Hooks/shims | `open()`/`fopen()` sobre rutas de "dispositivo" (`/dev/lbb`, `/dev/i2c/0`, `/dev/ttyS0/1`) | `src/os/syscall/` — intercepción a nivel de nuestra libc-shim, redirigiendo a estructuras en memoria o a ficheros sintéticos en `sdmc:/` |
| D. Patches | Ninguno identificado todavía para el título de referencia — se documentará por juego en su `GameProfile` si aparece | `src/patch/` |
| E. Depende de Windows | Ninguno — Lindbergh es Linux, ver `WINDOWS_COMPATIBILITY.md` | — |
| F. Depende del hardware arcade real | Placa base física, sensores de cabina, salida a monitor arcade nativo | No aplica en un port a consola — se sustituye por la pantalla/mandos de Switch |
| G. Depende del juego concreto | Shaders Cg específicos, tamaños de asset, rutas de fichero concretas | Se declara en `GameProfile` por título |
| H. Protección/DRM | "Security board" de Lindbergh son en realidad DIP switches + líneas test/service, sin keychip que volcar | Trivial de sintetizar, sin implicaciones de DRM real que sortear |

## Arquitectura de dispositivo virtual (según sección 10 del encargo)

```text
ArcadeDevice (interfaz)
   ├── VirtualDevice     — lo que el juego "ve" (p.ej. "JVS I/O con 2 monedas, volante de 8 bits")
   ├── DeviceBackend      — de dónde vienen realmente los datos en Switch (HidNpad, sensores del propio Switch)
   ├── DeviceProtocol     — cómo se serializa/deserializa (formato de paquete JVS real, para compatibilidad de perfil)
   └── DeviceProfile      — qué instancia de VirtualDevice+Backend usa cada GameProfile
```

Mapeo concreto para el MVP:

```text
Petición JVS del juego (coin, start, stick, gatillo)
      ↓
VirtualJvsIoBoard (implementa el protocolo JVS que el juego espera, en software, sin RS485 real)
      ↓
DeviceBackend::SwitchHid (lee HidNpadButtonSet / HidAnalogStickState vía libnx)
      ↓
Mapeo configurable por perfil (qué botón físico de Switch ↔ qué input JVS espera el juego)
```

No hace falta bus serie real ni adaptador RS485: como tanto el "cliente" (juego traducido) como el "servidor" (nuestra implementación JVS) corren dentro del mismo proceso Switch, el protocolo JVS se puede simular a nivel de estructura en memoria en vez de codificar/decodificar tramas de verdad — **excepto si en el roadmap se quiere soportar control físico arcade real vía USB-host de libnx**, en cuyo caso sí conviene implementar el framing JVS real (documentado públicamente por proyectos como OpenJVS/JVSCore) para hablar con una I/O board física conectada por USB-serie. Se deja como extensión, no como requisito del MVP.

## EEPROM / SRAM

**[CONFIRMADO por lindbergh-loader]**: no existe volcado real de keychip; el fichero `eeprom.bin` se sintetiza calculando CRCs de secciones `amSysDataRecord` a partir de constantes conocidas, y `sram.bin` se crea vacío en el primer arranque. Esto es directamente reproducible: `src/arcade/devices/eeprom.cpp` implementa la misma síntesis, sin necesidad de ingeniería inversa adicional más allá de replicar el algoritmo de CRC documentado.

## Explícitamente fuera de alcance del MVP

Gun controllers, volantes con force feedback, pedales, motion systems, EEPROM de I/O boards específicas (Type X, Namco): se documentan y se diseñan como `DeviceProfile` adicionales cuando el roadmap llegue a un juego que los necesite, no de antemano.
