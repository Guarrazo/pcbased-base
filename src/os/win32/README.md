# win32/ -- placeholder deliberado

Vacío a propósito. Ver `docs/WINDOWS_COMPATIBILITY.md`: el MVP (Sega
Lindbergh) no usa Windows, así que no hay ningún shim Win32 que escribir
todavía. Esta carpeta existe para que la separación de módulos del roadmap
(`docs/ARCHITECTURE.md` §3) esté prevista de antemano, no para que se rellene
de forma especulativa antes de tener un `GameProfile` Windows real que
determine qué subconjunto de `kernel32`/`user32`/`ntdll` hace falta.
